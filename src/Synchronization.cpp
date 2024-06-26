#include "../include/VKFS/Synchronization.h"

VKFS::Synchronization::Synchronization(VKFS::Device *device, VKFS::CommandBuffer *cmd, Swapchain* swapchain) : device(device), cmd(cmd), swapchain(swapchain) {
    imageAvailableSemaphores.resize(2);
    renderFinishedSemaphores.resize(2);
    inFlightFences.resize(2);
    computeInFlightFences.resize(2);
    computeFinishedSemaphores.resize(2);


    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < 2; i++) {
        if (vkCreateSemaphore(device->getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device->getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device->getDevice(), &semaphoreInfo, nullptr, &computeFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device->getDevice(), &fenceInfo, nullptr, &computeInFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("[VKFS] Failed to create synchronization objects for a frame!");
        }
    }
}

void VKFS::Synchronization::waitForFences() {
    vkWaitForFences(device->getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
}

uint32_t VKFS::Synchronization::acquireNextImage() {
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device->getDevice(), swapchain->getSwapchain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        swapchain->recreate(windowWidth, windowHeight);
        return -1;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("[VKFS] Failed to acquire swapchain image!");
    }

    return imageIndex;
}

VkCommandBuffer VKFS::Synchronization::getCommandBuffer() {
    return cmd->commandBuffers[currentFrame];
}

void VKFS::Synchronization::resetAll() {
    vkResetFences(device->getDevice(), 1, &inFlightFences[currentFrame]);
    vkResetCommandBuffer(cmd->commandBuffers[currentFrame], 0);
}

void VKFS::Synchronization::submit(uint32_t imageIndex) {

    if (windowWidth == -1 || windowHeight == -1) {
        throw std::runtime_error("[VKFS] The window size must be passed to the Sync object using the pushWindowSize() method every frame!");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphoresCompute[] = {imageAvailableSemaphores[currentFrame], computeFinishedSemaphores[currentFrame]};
    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};

    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = computeInUse ? 2 : 1;
    submitInfo.pWaitSemaphores = computeInUse ? waitSemaphoresCompute : waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    computeInUse = false;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd->commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(device->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("[VKFS] Failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapchain->getSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(device->getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        swapchain->recreate(windowWidth, windowHeight);
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("[VKFS] Failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % 2;
}

void VKFS::Synchronization::beginRecordingCommands() {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(getCommandBuffer(), &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("[VKFS] Failed to begin recording command buffer!");
    }
}

void VKFS::Synchronization::endRecordingCommands() {
    if (vkEndCommandBuffer(getCommandBuffer()) != VK_SUCCESS) {
        throw std::runtime_error("[VKFS] Failed to record command buffer!");
    }
}

uint32_t VKFS::Synchronization::getCurrentFrame() {
    return this->currentFrame;
}

void VKFS::Synchronization::pushWindowSize(int width, int height) {
    this->windowWidth = width;
    this->windowHeight = height;
}

void VKFS::Synchronization::submitCompute() {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd->computeBuffers[currentFrame];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &computeFinishedSemaphores[currentFrame];

    if (vkQueueSubmit(device->getComputeQueue(), 1, &submitInfo, computeInFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("[VKFS] Failed to submit compute command buffer!");
    };

}

void VKFS::Synchronization::resetCompute() {
    vkResetFences(device->getDevice(), 1, &computeInFlightFences[currentFrame]);
    vkResetCommandBuffer(getComputeCommandBuffer(), 0);
}

void VKFS::Synchronization::waitCompute() {
    vkWaitForFences(device->getDevice(), 1, &computeInFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
}

VkCommandBuffer VKFS::Synchronization::getComputeCommandBuffer() {
    return cmd->computeBuffers[currentFrame];
}

void VKFS::Synchronization::beginRecordingCompute() {
    computeInUse = true;
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(getComputeCommandBuffer(), &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("[VKFS] Failed to begin recording compute command buffer!");
    }
}

void VKFS::Synchronization::endRecordingCompute() {
    if (vkEndCommandBuffer(getComputeCommandBuffer()) != VK_SUCCESS) {
        throw std::runtime_error("[VKFS] Failed to record compute command buffer!");
    }
}
