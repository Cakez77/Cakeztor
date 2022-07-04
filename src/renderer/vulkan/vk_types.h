#pragma once

#include "logger.h"

#include <vulkan/vulkan.h>

#define VK_CHECK(result)                         \
    if (result != VK_SUCCESS)                    \
    {                                            \
        CAKEZ_ERROR("Vulkan Error: %d", result); \
        __debugbreak();                          \
    }

enum ImageID : u8
{
    IMAGE_ID_WHITE,
    IMAGE_ID_FONT,
};

struct Image
{
    ImageID ID;
    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;
};

struct Buffer
{
    VkBuffer buffer;
    VkDeviceMemory memory;
    u32 size;
    void *data;
};

struct DescriptorInfo
{
    union
    {
        VkDescriptorBufferInfo bufferInfo;
        VkDescriptorImageInfo imageInfo;
    };

    DescriptorInfo(VkSampler sampler, VkImageView imageView)
    {
        imageInfo.sampler = sampler;
        imageInfo.imageView = imageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    DescriptorInfo(VkBuffer buffer)
    {
        bufferInfo.buffer = buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;
    }

    DescriptorInfo(VkBuffer buffer, u32 offset, u32 range)
    {
        bufferInfo.buffer = buffer;
        bufferInfo.offset = offset;
        bufferInfo.range = range;
    }
};

struct Descriptor
{
    VkDescriptorSet set;
    ImageID imageID;
};

struct RenderCommand
{
    PushData pushData;
    u32 instanceCount;
    Descriptor *desc;
};
