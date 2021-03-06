/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2019, Gareth Francis
 * All rights reserved.
 */

#include "framebuffer.h"

#include "windowintegration.h"

FrameBuffer::FrameBuffer(vk::Device& device, const WindowIntegration& windowIntegration, vk::RenderPass& renderPass)
{
  createFrameBuffers(device, windowIntegration, renderPass);
}

void FrameBuffer::createFrameBuffers( vk::Device& device, const WindowIntegration& windowIntegration, vk::RenderPass& renderPass ) {
  if( !mFrameBuffers.empty() ) throw std::runtime_error("Framenbuffer::createFramebuffers: Already initialised");
  for( auto i = 0u; i < windowIntegration.swapChainSize(); ++i ) {
    // The attachments for the framebuffer
    // The depth and colour attachments can be shared as long as we don't need to read the contents - Only 1 frame will be rendered at once and it's cleared beforehand
    // swapchain attachments are one-per, as they cannot be shared between frames-in-flight
    // Note: Order here must match the attach order on the render pass
    std::vector<vk::ImageView> attachments;
    attachments.emplace_back(windowIntegration.sampleImageView());
    attachments.emplace_back(windowIntegration.depthImageView());
    attachments.emplace_back(windowIntegration.swapChainImageViews()[i].get());

    auto info = vk::FramebufferCreateInfo()
        .setRenderPass(renderPass) // Compatible with this render pass (don't use it with any other)
        .setAttachmentCount(attachments.size())
        .setPAttachments(attachments.size() ? attachments.data() : nullptr)
        .setWidth(windowIntegration.swapChainExtent().width)
        .setHeight(windowIntegration.swapChainExtent().height)
        .setLayers(1)
        ;

    auto fb = device.createFramebufferUnique(info);
    if( !fb ) throw std::runtime_error("Failed to create framebuffer");
    mFrameBuffers.emplace_back( std::move(fb) );
  }
}
