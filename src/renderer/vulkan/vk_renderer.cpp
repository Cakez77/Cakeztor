#include <vulkan/vulkan.h>
#ifdef WINDOWS_BUILD
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#elif LINUX_BUILD
#endif

// App
#include "logger.h"
#include "platform.h"
#include "my_math.h"

// Renderer
#include "shared_render_types.h"
#include "vk_types.h"
#include "vk_init.cpp"
#include "vk_util.cpp"
#include "vk_shader_util.cpp"

u32 constexpr MAX_IMAGES = 10;
u32 constexpr MAX_DESCRIPTORS = 10;
u32 constexpr MAX_RENDER_COMMANDS = 10;
u32 constexpr MAX_TRANSFORMS = 5000;
u32 constexpr MAX_MATERIALS = 100;
u32 constexpr FONT_PADDING = 2;

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity,
    VkDebugUtilsMessageTypeFlagsEXT msgFlags,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData)
{
    // CAKEZ_ERROR(pCallbackData->pMessage);
    CAKEZ_ASSERT(0, pCallbackData->pMessage);
    return false;
}

struct Glyph
{
    Vec2 size;
    float topV;
    float bottomV;
    float leftU;
    float rightU;
    float xOff;
    float yOff;
};

struct GlyphCache
{
    u32 fontSize;
    u32 fontBitmapWidth;
    u32 fontBitmapHeight;
    Glyph glyphs[255];
};

struct VkContext
{
    bool vSync;
    VkExtent2D screenSize;

    GlyphCache glyphCache;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surfaceFormat;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSwapchainKHR swapchain;
    VkRenderPass renderPass;
    VkCommandPool commandPool;
    VkCommandBuffer cmd;

    u32 imageCount;
    Image images[MAX_IMAGES];

    u32 descCount;
    Descriptor descriptors[MAX_DESCRIPTORS];

    u32 renderCommandCount;
    RenderCommand renderCommands[MAX_RENDER_COMMANDS];

    u32 transformCount;
    Transform transforms[MAX_TRANSFORMS];

    u32 materialCount;
    MaterialData materials[MAX_MATERIALS];

    Buffer stagingBuffer;
    Buffer transformStorageBuffer;
    Buffer materialStorageBuffer;
    Buffer globalUBO;
    Buffer indexBuffer;

    VkDescriptorPool descPool;

    VkSampler sampler;
    VkDescriptorSetLayout setLayout;
    VkPipelineLayout pipeLayout;
    VkPipeline pipeline;

    VkSemaphore aquireSemaphore;
    VkSemaphore submitSemaphore;
    VkFence imgAvailableFence;

    u32 scImgCount;
    VkImage scImages[5];
    VkImageView scImgViews[5];
    VkFramebuffer framebuffers[5];

    int graphicsIdx;
};

Image *vk_create_image(VkContext *vkcontext, ImageID imageID, 
                        char* data, u32 width, u32 height, 
                        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM)
{
    Image *image = 0;
    if (vkcontext->imageCount < MAX_IMAGES)
    {
        u32 textureSize = width * height;
        if(format == VK_FORMAT_R8G8B8A8_UNORM)
        {
            textureSize *= 4;
        }

        vk_copy_to_buffer(&vkcontext->stagingBuffer, data, textureSize);

        //TODO: Assertions
        image = &vkcontext->images[vkcontext->imageCount];
        *image = vk_allocate_image(vkcontext->device,
                                   vkcontext->gpu,
                                   width,
                                   height,
                                   format);
        image->ID = imageID;

        if (image->image != VK_NULL_HANDLE && image->memory != VK_NULL_HANDLE)
        {
            VkCommandBuffer cmd;
            VkCommandBufferAllocateInfo cmdAlloc = cmd_alloc_info(vkcontext->commandPool);
            VK_CHECK(vkAllocateCommandBuffers(vkcontext->device, &cmdAlloc, &cmd));

            VkCommandBufferBeginInfo beginInfo = cmd_begin_info();
            VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

            VkImageSubresourceRange range = {};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.layerCount = 1;
            range.levelCount = 1;

            // Transition Layout to Transfer optimal
            VkImageMemoryBarrier imgMemBarrier = {};
            imgMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imgMemBarrier.image = image->image;
            imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            imgMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imgMemBarrier.subresourceRange = range;

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0,
                                 1, &imgMemBarrier);

            VkBufferImageCopy copyRegion = {};
            copyRegion.imageExtent = {width, height, 1};
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            vkCmdCopyBufferToImage(cmd, vkcontext->stagingBuffer.buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imgMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0,
                                 1, &imgMemBarrier);

            VK_CHECK(vkEndCommandBuffer(cmd));

            VkFence uploadFence;
            VkFenceCreateInfo fenceInfo = fence_info();
            VK_CHECK(vkCreateFence(vkcontext->device, &fenceInfo, 0, &uploadFence));

            VkSubmitInfo submitInfo = submit_info(&cmd);
            VK_CHECK(vkQueueSubmit(vkcontext->graphicsQueue, 1, &submitInfo, uploadFence));

            // Create Image View while waiting on the GPU
            {
                VkImageViewCreateInfo viewInfo = {};
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = image->image;
                viewInfo.format = format;
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                viewInfo.subresourceRange.layerCount = 1;
                viewInfo.subresourceRange.levelCount = 1;
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

                VK_CHECK(vkCreateImageView(vkcontext->device, &viewInfo, 0, &image->view));
            }

            VK_CHECK(vkWaitForFences(vkcontext->device, 1, &uploadFence, true, UINT64_MAX));
        }
        else
        {
            CAKEZ_ASSERT(image->image != VK_NULL_HANDLE,
                         "Failed to allocate Memory for Image!");
            CAKEZ_ASSERT(image->memory != VK_NULL_HANDLE,
                         "Failed to allocate Memory for Image!");
            image = 0;
        }
    }
    else
    {
        CAKEZ_ASSERT(0, "Reached Maximum amount of Images");
    }

    if(image)
    {
        vkcontext->imageCount++;
    }

    return image;
}

Image *vk_get_image(VkContext *vkcontext, ImageID imageID)
{
    Image *image = 0;
    for (u32 i = 0; i < vkcontext->imageCount; i++)
    {
        Image *img = &vkcontext->images[i];

        if (img->ID == imageID)
        {
            image = img;
            break;
        }
    }

    return image;
}

internal Descriptor *vk_create_descriptor(VkContext *vkcontext, ImageID imageID)
{
    Descriptor *desc = 0;

    if (vkcontext->descCount < MAX_DESCRIPTORS)
    {
        desc = &vkcontext->descriptors[vkcontext->descCount];
        *desc = {};
        desc->imageID = imageID;

        Image *image = vk_get_image(vkcontext, imageID);

        if(image)
        {
            // Allocation
            {
                VkDescriptorSetAllocateInfo allocInfo = {};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.pSetLayouts = &vkcontext->setLayout;
                allocInfo.descriptorSetCount = 1;
                allocInfo.descriptorPool = vkcontext->descPool;
                VK_CHECK(vkAllocateDescriptorSets(vkcontext->device, &allocInfo, &desc->set));
            }

            if (desc->set != VK_NULL_HANDLE)
            {
                // Update Descriptor Set
                {

                    DescriptorInfo descInfos[] = {
                        DescriptorInfo(vkcontext->globalUBO.buffer),
                        DescriptorInfo(vkcontext->transformStorageBuffer.buffer),
                        DescriptorInfo(vkcontext->sampler, image->view),
                        DescriptorInfo(vkcontext->materialStorageBuffer.buffer)};

                    VkWriteDescriptorSet writes[] = {
                        write_set(desc->set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                &descInfos[0], 0, 1),
                        write_set(desc->set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                &descInfos[1], 1, 1),
                        write_set(desc->set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                &descInfos[2], 2, 1),
                        write_set(desc->set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                &descInfos[3], 3, 1)};

                    vkUpdateDescriptorSets(vkcontext->device, ArraySize(writes), writes, 0, 0);

                    // Actually add the descriptor;
                    vkcontext->descCount++;
                }
            }
            else
            {
                desc = 0;
                CAKEZ_ASSERT(0, "Failed to Allocate Descriptor Set for Image ID: %d", imageID);
                CAKEZ_ERROR("Failed to Allocate Descriptor Set for Image ID: %d", imageID);
            }
        }
        else
        {
            CAKEZ_ASSERT(0, "Failed to get Image for Image ID: %d", imageID);
            CAKEZ_ERROR("Failed to get Image for Image ID: %d", imageID);
        }
    }
    else
    {
        CAKEZ_ASSERT(0, "Reached Maximum amount of Descriptors");
    }

    return desc;
}

internal Descriptor *vk_get_descriptor(VkContext *vkcontext, ImageID imageID)
{
    Descriptor *desc = 0;

    for (u32 i = 0; i < vkcontext->descCount; i++)
    {
        Descriptor *d = &vkcontext->descriptors[i];

        if (d->imageID == imageID)
        {
            desc = d;
            break;
        }
    }

    return desc;
}

internal RenderCommand *vk_add_render_command(VkContext *vkcontext, Descriptor *desc)
{
    CAKEZ_ASSERT(desc, "No Descriptor supplied!");

    RenderCommand *rc = 0;

    if (vkcontext->renderCommandCount < MAX_RENDER_COMMANDS)
    {
        rc = &vkcontext->renderCommands[vkcontext->renderCommandCount++];
        *rc = {};
        rc->desc = desc;
    }
    else
    {
        CAKEZ_ERROR(0, "Reached Maximum amount of Render Commands");
        CAKEZ_ASSERT(0, "Reached Maximum amount of Render Commands");
    }

    return rc;
}

internal u32 get_material_idx(VkContext*vkcontext, Vec4 color)
{
    u32 materialIdxOut = INVALID_IDX;
    for(u32 materialIdx = 0; 
    materialIdx < vkcontext->materialCount;
    materialIdx++)
    {
        if(vkcontext->materials[materialIdx].color == color)
        {
            materialIdxOut = materialIdx;
        }
    }

    if(materialIdxOut == INVALID_IDX && 
        vkcontext->materialCount < MAX_MATERIALS)
    {
        vkcontext->materials[vkcontext->materialCount].color = color;
        materialIdxOut = vkcontext->materialCount++;
    }

    return materialIdxOut;
}

internal void vk_add_transform(
    VkContext *vkcontext,
    ImageID imageID,
    Vec2 pos,
    Vec2 size,
    Vec4 color,
    u32 animationIdx)
{
    if (vkcontext->transformCount < MAX_TRANSFORMS)
    {
        u32 materialIdx = get_material_idx(vkcontext, color);

        Transform t = {};
        t.materialIdx = materialIdx;
        t.imageID = imageID;
        t.xPos = pos.x;
        t.yPos = pos.y;
        t.sizeX = size.x;
        t.sizeY = size.y;
        t.animationIdx = animationIdx;

        if(imageID == IMAGE_ID_FONT)
        {
            Glyph g = vkcontext->glyphCache.glyphs[animationIdx];
            t.topV = g.topV;
            t.bottomV = g.bottomV;
            t.leftU = g.leftU;
            t.rightU = g.rightU;
        }
        else
        {
            t.topV = 0.0f;
            t.bottomV = 1.0f;
            t.leftU = 0.0f;
            t.rightU = 1.0f;
        }

        vkcontext->transforms[vkcontext->transformCount++] = t;
    }
    else
    {
        CAKEZ_ASSERT(0, "Reached maximum amount of transforms!");
    }
}

internal void vk_create_swapchain_objects(
    VkContext *vkcontext,
    bool vSync)
{
    // Swapchain
    {
        VkSurfaceCapabilitiesKHR surfaceCaps = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkcontext->gpu, vkcontext->surface, &surfaceCaps));
        u32 imgCount = surfaceCaps.minImageCount + 1;
        imgCount = imgCount > surfaceCaps.maxImageCount ? imgCount - 1 : imgCount;

        VkSwapchainCreateInfoKHR scInfo = {};
        scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        scInfo.surface = vkcontext->surface;
        scInfo.imageFormat = vkcontext->surfaceFormat.format;
        scInfo.preTransform = surfaceCaps.currentTransform;
        scInfo.imageExtent = surfaceCaps.currentExtent;
        scInfo.minImageCount = imgCount;
        scInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // This is guaranteed to be supported
        scInfo.imageArrayLayers = 1;

        VK_CHECK(vkCreateSwapchainKHR(vkcontext->device, &scInfo, 0, &vkcontext->swapchain));

        VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device, vkcontext->swapchain, &vkcontext->scImgCount, 0));
        VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device, vkcontext->swapchain, &vkcontext->scImgCount, vkcontext->scImages));

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.format = vkcontext->surfaceFormat.format;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        for (u32 i = 0; i < vkcontext->scImgCount; i++)
        {
            viewInfo.image = vkcontext->scImages[i];
            VK_CHECK(vkCreateImageView(vkcontext->device, &viewInfo, 0, &vkcontext->scImgViews[i]));
        }
    }


    // Frame Buffers
    {
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = vkcontext->renderPass;
        fbInfo.width = vkcontext->screenSize.width;
        fbInfo.height = vkcontext->screenSize.height;
        fbInfo.layers = 1;
        fbInfo.attachmentCount = 1;

        for (u32 i = 0; i < vkcontext->scImgCount; i++)
        {
            fbInfo.pAttachments = &vkcontext->scImgViews[i];
            VK_CHECK(vkCreateFramebuffer(vkcontext->device, &fbInfo, 0, &vkcontext->framebuffers[i]));
        }
    }
}

internal void vk_resize_swapchain(
    VkContext *vkcontext,
    InputState *input,
    bool vSync)
{
    if (vkcontext->device != VK_NULL_HANDLE)
    {
        vkcontext->screenSize.width = (u32)input->screenSize.x;
        vkcontext->screenSize.height = (u32)input->screenSize.y;

        // Make sure the device is not working anymore
        VK_CHECK(vkDeviceWaitIdle(vkcontext->device));

        // Copy new screensize to the global UBO
        {
            GlobalData globalData = {};
            globalData.screenSizeX = vkcontext->screenSize.width;
            globalData.screenSizeY = vkcontext->screenSize.height;

            vk_copy_to_buffer(
                &vkcontext->globalUBO,
                &globalData,
                sizeof(GlobalData));
        }

        // Cleanup swapchain
        {
            // Destroy the Framebuffers and Image Views
            for (uint32_t i = 0; i < (uint32_t)vkcontext->scImgCount; i++)
            {
                // Destroy Post Processing Framebuffers + Views
                vkDestroyFramebuffer(vkcontext->device, vkcontext->framebuffers[i], 0);
                vkDestroyImageView(vkcontext->device, vkcontext->scImgViews[i], 0);
            }

            vkcontext->scImgCount = 0;
            vkDestroySwapchainKHR(vkcontext->device, vkcontext->swapchain, 0);
        }

        vk_create_swapchain_objects(vkcontext, vSync);
    }
}

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

internal void vk_init_font(VkContext*vkcontext, char* bitmap, 
                        u32 imgWidth, u32 fontSize)
{
    vkcontext->glyphCache.fontSize = fontSize;
    vkcontext->glyphCache.fontBitmapWidth = imgWidth;
    vkcontext->glyphCache.fontBitmapHeight = imgWidth;

    u32 fileSize;
    char* buffer = platform_read_file("fonts/arial.ttf", &fileSize);

    stbtt_fontinfo font;
    stbtt_InitFont(&font, (unsigned char*)buffer, 0);

    memset(bitmap, 0, imgWidth * imgWidth);

    float scaleY;
    scaleY = stbtt_ScaleForPixelHeight(&font, fontSize);

    u32 glyphRowCount = 0;
    u32 bitmapColIdx = FONT_PADDING;
    s32 width, height, xOff, yOff;

    for(unsigned char c = 0; c < 127; c++)
    {
        unsigned char* glyphBitmap = 0;

        // This allocates on the Heap
        glyphBitmap = stbtt_GetCodepointBitmap(&font, 0, scaleY, c, &width, &height, &xOff, &yOff);

        Glyph* glyph = &vkcontext->glyphCache.glyphs[c];
        glyph->size = {(float)width + FONT_PADDING, (float)height + FONT_PADDING};
        glyph->xOff = xOff;
        glyph->yOff = yOff;


        if(bitmapColIdx + FONT_PADDING + width >= imgWidth)
        {
            glyphRowCount++;
            bitmapColIdx = 0;
        }

        u32 bitmapRowIdx = (glyphRowCount * fontSize + FONT_PADDING);

        // Write the glyph to the grayscale image
        u32 startBitmapIdx = bitmapRowIdx * imgWidth + bitmapColIdx;
        for (u32 y = 0; y < height; y++)
        {
            for (u32 x = 0; x < width; x++)
            {
                unsigned char glyphC = glyphBitmap[y * width + x];

                u32 subIdx = startBitmapIdx + y * imgWidth + x;
                bitmap[subIdx] = glyphC;
            }
        }

        // Calculate the UV Coordinates of the Glyph
        {
            int bitmapRowIdxGlyph = bitmapRowIdx - FONT_PADDING / 2;
            int glyphHeight = height + FONT_PADDING; // 2 * FONT_PADDING / 2
            int bitmapColIdxGlyph = bitmapColIdx - FONT_PADDING / 2;
            int glyphWidth = width + FONT_PADDING; // 2 * FONT_PADDING / 2
            glyph->topV = (float)bitmapRowIdxGlyph / (float)imgWidth;
            glyph->bottomV = float(bitmapRowIdxGlyph + glyphHeight) / (float)imgWidth;
            glyph->leftU = (float)bitmapColIdxGlyph / (float)imgWidth;
            glyph->rightU = float(bitmapColIdxGlyph + glyphWidth) / (float)imgWidth;
        }

        bitmapColIdx += FONT_PADDING + width;
    }

    vk_create_image(vkcontext, IMAGE_ID_FONT, bitmap, 
                    imgWidth, imgWidth, VK_FORMAT_R8_UNORM);
}

bool vk_init(VkContext *vkcontext, void *window, bool vSync)
{
    vkcontext->vSync = vSync;

    vk_compile_shader("shaders/shader.vert", "shaders/compiled/shader.vert.spv");
    vk_compile_shader("shaders/shader.frag", "shaders/compiled/shader.frag.spv");

    platform_get_window_size(&vkcontext->screenSize.width, &vkcontext->screenSize.height);

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Cakeztor";
    appInfo.pEngineName = "Cakeztor";

    char *extensions[] = {
#ifdef WINDOWS_BUILD
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif LINUX_BUILD
#endif
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME};

    char *layers[]{
        "VK_LAYER_KHRONOS_validation"};

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.ppEnabledExtensionNames = extensions;
    instanceInfo.enabledExtensionCount = ArraySize(extensions);
    instanceInfo.ppEnabledLayerNames = layers;
    instanceInfo.enabledLayerCount = ArraySize(layers);
    VK_CHECK(vkCreateInstance(&instanceInfo, 0, &vkcontext->instance));

    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkcontext->instance, "vkCreateDebugUtilsMessengerEXT");

    if (vkCreateDebugUtilsMessengerEXT)
    {
        VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
        debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debugInfo.pfnUserCallback = vk_debug_callback;

        vkCreateDebugUtilsMessengerEXT(vkcontext->instance, &debugInfo, 0, &vkcontext->debugMessenger);
    }
    else
    {
        return false;
    }

    // Create Surface
    {
#ifdef WINDOWS_BUILD
        VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.hwnd = (HWND)window;
        surfaceInfo.hinstance = GetModuleHandleA(0);
        VK_CHECK(vkCreateWin32SurfaceKHR(vkcontext->instance, &surfaceInfo, 0, &vkcontext->surface));
#elif LINUX_BUILD
#endif
    }

    // Choose GPU
    {
        vkcontext->graphicsIdx = -1;

        u32 gpuCount = 0;
        //TODO: Suballocation from Main Allocation
        VkPhysicalDevice gpus[10];
        VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance, &gpuCount, 0));
        VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance, &gpuCount, gpus));

        for (u32 i = 0; i < gpuCount; i++)
        {
            VkPhysicalDevice gpu = gpus[i];

            u32 queueFamilyCount = 0;
            //TODO: Suballocation from Main Allocation
            VkQueueFamilyProperties queueProps[10];
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, 0);
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueProps);

            for (u32 j = 0; j < queueFamilyCount; j++)
            {
                if (queueProps[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    VkBool32 surfaceSupport = VK_FALSE;
                    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, j, vkcontext->surface, &surfaceSupport));

                    if (surfaceSupport)
                    {
                        vkcontext->graphicsIdx = j;
                        vkcontext->gpu = gpu;
                        break;
                    }
                }
            }
        }

        if (vkcontext->graphicsIdx < 0)
        {
            return false;
        }
    }

    // Logical Device
    {
        float queuePriority = 1.0f;

        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = vkcontext->graphicsIdx;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        char *extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkDeviceCreateInfo deviceInfo = {};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.ppEnabledExtensionNames = extensions;
        deviceInfo.enabledExtensionCount = ArraySize(extensions);

        VK_CHECK(vkCreateDevice(vkcontext->gpu, &deviceInfo, 0, &vkcontext->device));

        // Get Graphics Queue
        vkGetDeviceQueue(vkcontext->device, vkcontext->graphicsIdx, 0, &vkcontext->graphicsQueue);
    }

    // Surface Format
    {
        u32 formatCount = 0;
        VkSurfaceFormatKHR surfaceFormats[10];
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkcontext->gpu, vkcontext->surface, &formatCount, 0));
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkcontext->gpu, vkcontext->surface, &formatCount, surfaceFormats));

        for (u32 i = 0; i < formatCount; i++)
        {
            VkSurfaceFormatKHR format = surfaceFormats[i];

            if (format.format == VK_FORMAT_B8G8R8A8_SRGB)
            {
                vkcontext->surfaceFormat = format;
                break;
            }
        }
    }

    // Render Pass
    {
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = vkcontext->surfaceFormat.format;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription attachments[] = {
            colorAttachment};

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0; // This is an index into the attachments array
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpassDesc = {};
        subpassDesc.colorAttachmentCount = 1;
        subpassDesc.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo rpInfo = {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.pAttachments = attachments;
        rpInfo.attachmentCount = ArraySize(attachments);
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &subpassDesc;

        VK_CHECK(vkCreateRenderPass(vkcontext->device, &rpInfo, 0, &vkcontext->renderPass));
    }

    // Swapchain
    {
        vk_create_swapchain_objects(vkcontext, vSync);
    }

    // Command Pool
    {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = vkcontext->graphicsIdx;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK(vkCreateCommandPool(vkcontext->device, &poolInfo, 0, &vkcontext->commandPool));
    }

    // Command Buffer
    {
        VkCommandBufferAllocateInfo allocInfo = cmd_alloc_info(vkcontext->commandPool);
        VK_CHECK(vkAllocateCommandBuffers(vkcontext->device, &allocInfo, &vkcontext->cmd));
    }

    // Sync Objects
    {
        VkSemaphoreCreateInfo semaInfo = {};
        semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaInfo, 0, &vkcontext->aquireSemaphore));
        VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaInfo, 0, &vkcontext->submitSemaphore));

        VkFenceCreateInfo fenceInfo = fence_info(VK_FENCE_CREATE_SIGNALED_BIT);
        VK_CHECK(vkCreateFence(vkcontext->device, &fenceInfo, 0, &vkcontext->imgAvailableFence));
    }

    // Create Descriptor Set Layouts
    {
        VkDescriptorSetLayoutBinding layoutBindings[] = {
            layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1, 0),
            layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1, 1),
            layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 2),
            layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 3)};

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = ArraySize(layoutBindings);
        layoutInfo.pBindings = layoutBindings;

        VK_CHECK(vkCreateDescriptorSetLayout(vkcontext->device, &layoutInfo, 0, &vkcontext->setLayout));
    }

    // Create Pipeline Layout
    {
        VkPushConstantRange pushConstant = {};
        pushConstant.size = sizeof(PushData);
        pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &vkcontext->setLayout;
        layoutInfo.pPushConstantRanges = &pushConstant;
        layoutInfo.pushConstantRangeCount = 1;
        VK_CHECK(vkCreatePipelineLayout(vkcontext->device, &layoutInfo, 0, &vkcontext->pipeLayout));
    }

    // Create a Pipeline
    {

        VkPipelineVertexInputStateCreateInfo vertexInputState = {};
        vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlendState = {};
        colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendState.pAttachments = &colorBlendAttachment;
        colorBlendState.attachmentCount = 1;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport viewport = {};
        viewport.maxDepth = 1.0;

        VkRect2D scissor = {};

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.pViewports = &viewport;
        viewportState.viewportCount = 1;
        viewportState.pScissors = &scissor;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizationState = {};
        rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationState.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampleState = {};
        multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkShaderModule vertexShader, fragmentShader;

        // Vertex Shader
        {
            u32 lengthInBytes;
            u32 *vertexCode = (u32 *)platform_read_file("shaders/compiled/shader.vert.spv", &lengthInBytes);

            VkShaderModuleCreateInfo shaderInfo = {};
            shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderInfo.pCode = vertexCode;
            shaderInfo.codeSize = lengthInBytes;
            VK_CHECK(vkCreateShaderModule(vkcontext->device, &shaderInfo, 0, &vertexShader));
        }

        // Fragment Shader
        {
            u32 lengthInBytes;
            u32 *fragmentCode = (u32 *)platform_read_file("shaders/compiled/shader.frag.spv", &lengthInBytes);

            VkShaderModuleCreateInfo shaderInfo = {};
            shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderInfo.pCode = fragmentCode;
            shaderInfo.codeSize = lengthInBytes;
            VK_CHECK(vkCreateShaderModule(vkcontext->device, &shaderInfo, 0, &fragmentShader));
        }

        VkPipelineShaderStageCreateInfo vertStage = {};
        vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.pName = "main";
        vertStage.module = vertexShader;

        VkPipelineShaderStageCreateInfo fragStage = {};
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.pName = "main";
        fragStage.module = fragmentShader;

        VkPipelineShaderStageCreateInfo shaderStages[2]{
            vertStage,
            fragStage};

        VkDynamicState dynamicStates[]{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pDynamicStates = dynamicStates;
        dynamicState.dynamicStateCount = ArraySize(dynamicStates);

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.renderPass = vkcontext->renderPass;
        pipelineInfo.pVertexInputState = &vertexInputState;
        pipelineInfo.pColorBlendState = &colorBlendState;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizationState;
        pipelineInfo.pMultisampleState = &multisampleState;
        pipelineInfo.stageCount = ArraySize(shaderStages);
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = vkcontext->pipeLayout;
        pipelineInfo.pStages = shaderStages;

        VK_CHECK(vkCreateGraphicsPipelines(vkcontext->device, 0, 1, &pipelineInfo, 0, &vkcontext->pipeline));

        vkDestroyShaderModule(vkcontext->device, vertexShader, 0);
        vkDestroyShaderModule(vkcontext->device, fragmentShader, 0);
    }

    // Staging Buffer
    {
        vkcontext->stagingBuffer = vk_allocate_buffer(vkcontext->device, vkcontext->gpu,
                                                      MB(1), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }

    // Create Image
    {
        u8 white[] ={255, 255, 255, 255};

        Image *image = vk_create_image(vkcontext, IMAGE_ID_WHITE, (char*)white, 1, 1);

        if (!image->memory || !image->image || !image->view)
        {
            CAKEZ_ASSERT(0, "Failed to allocate White Texure");
            CAKEZ_FATAL("Failed to allocate White Texure");
            return false;
        }
    }

    // Create Sampler
    {
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.magFilter = VK_FILTER_NEAREST;

        VK_CHECK(vkCreateSampler(vkcontext->device, &samplerInfo, 0, &vkcontext->sampler));
    }

    // Create Transform Storage Buffer
    {
        vkcontext->transformStorageBuffer = vk_allocate_buffer(
            vkcontext->device,
            vkcontext->gpu,
            sizeof(Transform) * MAX_TRANSFORMS,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }

    // Create Material Storage Buffer
    {
        vkcontext->materialStorageBuffer = vk_allocate_buffer(
            vkcontext->device,
            vkcontext->gpu,
            sizeof(MaterialData) * MAX_MATERIALS,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }

    // Create Global Uniform Buffer Object
    {
        vkcontext->globalUBO = vk_allocate_buffer(
            vkcontext->device,
            vkcontext->gpu,
            sizeof(GlobalData),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        GlobalData globalData = {
            (int)vkcontext->screenSize.width,
            (int)vkcontext->screenSize.height};

        vk_copy_to_buffer(&vkcontext->globalUBO, &globalData, sizeof(globalData));
    }

    // Create Index Buffer
    {
        vkcontext->indexBuffer = vk_allocate_buffer(
            vkcontext->device,
            vkcontext->gpu,
            sizeof(u32) * 6,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        // Copy Indices to the buffer
        {
            u32 indices[] = {0, 1, 2, 2, 3, 0};
            vk_copy_to_buffer(&vkcontext->indexBuffer, &indices, sizeof(u32) * 6);
        }
    }

    // Create Descriptor Pool
    {
        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = MAX_DESCRIPTORS;
        poolInfo.poolSizeCount = ArraySize(poolSizes);
        poolInfo.pPoolSizes = poolSizes;
        VK_CHECK(vkCreateDescriptorPool(vkcontext->device, &poolInfo, 0, &vkcontext->descPool));
    }

    // Create Descriptor Set

    return true;
}

internal void vk_draw_rect(VkContext* vkcontext, ImageID imageID,
        Vec2 pos, Vec2 size, Vec4 color = {1.0f, 1.0f, 1.0f, 1.0f}, u32 animationIdx = 0)
{
    vk_add_transform(vkcontext, imageID, pos, size, color, animationIdx);
}

internal Vec2 vk_render_text(VkContext* vkcontext, unsigned char* text, Vec2 origin)
{
    float originalOriginX = origin.x;
    while(unsigned char c = *(text++))
    {
        Glyph g = vkcontext->glyphCache.glyphs[c];
        switch(c)
        {
            case ' ':
            origin.x += vkcontext->glyphCache.fontSize / 2;
            break;

            case '\n':
            case '\r':
            origin.y += vkcontext->glyphCache.fontSize;
            origin.x = originalOriginX;
            break;

            default:
                vk_draw_rect(vkcontext, IMAGE_ID_FONT, 
                            origin + Vec2{g.xOff, g.yOff}, 
                            g.size, {1.0f, 1.0f, 1.0f, 1.0f}, c);

                origin.x += g.size.x;
        }
    }
    return origin;
}


bool vk_render(VkContext *vkcontext, InputState* input, AppState* app)
{
    u32 imgIdx;

    // We wait on the GPU to be done with the work
    VK_CHECK(vkWaitForFences(vkcontext->device, 1, &vkcontext->imgAvailableFence, 
                                VK_TRUE, UINT64_MAX));

    float fontSize = (float)vkcontext->glyphCache.fontSize;
    Vec2 origin = vk_render_text(vkcontext, app->buffer, {40.0f, 40.0f});
    vk_draw_rect(vkcontext, IMAGE_ID_WHITE, origin + Vec2{0.0f, -fontSize * 0.8f}, 
        {fontSize / 2.0f, fontSize},
        {1.0f, 1.0f, 1.0f, 0.5f});

    Descriptor *currentDesc = 0;
    RenderCommand *rc = 0;
    for(uint32_t transformIdx = 0; transformIdx < vkcontext->transformCount; transformIdx++)
    {
        Transform* t = &vkcontext->transforms[transformIdx];

        Descriptor *desc = vk_get_descriptor(vkcontext, (ImageID)t->imageID);
        if(!desc)
        {
            desc = vk_create_descriptor(vkcontext, (ImageID)t->imageID);
        }

        if(desc) 
        {
            if(currentDesc != desc)
            {
                currentDesc = desc;
                rc = vk_add_render_command(vkcontext, desc);
                rc->pushData.transformIdx = transformIdx;

                if (rc)
                {
                    rc->instanceCount = 1;
                }
                else
                {
                    break;
                }
            }
            else
            {
                rc->instanceCount++;
            }
        }
    }


    // // UI Rendering
    // {
    //     for (u32 uiEleIdx = 0;
    //          uiEleIdx < ui->uiElementCount;
    //          uiEleIdx++)
    //     {
    //         UIElement uiElement = ui->uiElements[uiEleIdx];

    //         Descriptor *desc = vk_get_descriptor(vkcontext, uiElement.assetTypeID);

    //         if (desc)
    //         {
    //             RenderCommand *rc = vk_add_render_command(vkcontext, desc);
    //             if (rc)
    //             {
    //                 rc->instanceCount = 1;
    //             }
    //         }

    //         vk_add_transform(vkcontext, gameState, uiElement.assetTypeID,
    //                          {1.0f, 1.0f, 1.0f, 1.0f}, uiElement.rect.pos,
    //                          uiElement.animationIdx);
    //     }

    //     if (ui->labelCount)
    //     {
    //         Descriptor *desc = vk_get_descriptor(vkcontext, ASSET_SPRITE_FONT_ATLAS);
    //         if (desc)
    //         {
    //             RenderCommand *rc = vk_add_render_command(vkcontext, desc);
    //             if (rc)
    //             {
    //                 for (u32 labelIdx = 0; labelIdx < ui->labelCount; labelIdx++)
    //                 {
    //                     Label *l = &ui->labels[labelIdx];

    //                     vk_render_text(vkcontext, gameState, l->text, l->pos);

    //                     // TODO: This can break if text == 0!
    //                     rc->instanceCount += strlen(l->text);
    //                 }
    //             }
    //         }
    //     }
    // }

    // Copy Data to buffers
    {
        vk_copy_to_buffer(&vkcontext->transformStorageBuffer, &vkcontext->transforms, sizeof(Transform) * vkcontext->transformCount);
        vkcontext->transformCount = 0;

        vk_copy_to_buffer(&vkcontext->materialStorageBuffer, vkcontext->materials, 
                        sizeof(MaterialData) * vkcontext->materialCount);
        vkcontext->materialCount = 0;
    }

    // This waits on the timeout until the image is ready, if timeout reached -> VK_TIMEOUT
    VkResult result = vkAcquireNextImageKHR(vkcontext->device, vkcontext->swapchain, UINT64_MAX, vkcontext->aquireSemaphore, 0, &imgIdx);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) // i.e. we changed the window size
    {
        CAKEZ_WARN("Acquire next Image resulted in VK_ERROR_OUT_OF_DATE_KHR, recreating the Swapchain!");
        vk_resize_swapchain(vkcontext, input, false);
        return false;
    }
    else
    {
        VK_CHECK(result);
    }

    VK_CHECK(vkResetFences(vkcontext->device, 1, &vkcontext->imgAvailableFence));

    VkCommandBuffer cmd = vkcontext->cmd;
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo = cmd_begin_info();
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // Clear Color to Yellow
    VkClearValue clearValue = {};
    clearValue.color = {0, 0, 0, 1};

    VkRenderPassBeginInfo rpBeginInfo = {};
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderArea.extent = vkcontext->screenSize;
    rpBeginInfo.clearValueCount = 1;
    rpBeginInfo.pClearValues = &clearValue;
    rpBeginInfo.renderPass = vkcontext->renderPass;
    rpBeginInfo.framebuffer = vkcontext->framebuffers[imgIdx];
    vkCmdBeginRenderPass(cmd, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {};
    viewport.maxDepth = 1.0f;
    viewport.width = vkcontext->screenSize.width;
    viewport.height = vkcontext->screenSize.height;

    VkRect2D scissor = {};
    scissor.extent = vkcontext->screenSize;

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindIndexBuffer(cmd, vkcontext->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkcontext->pipeline);

    // Render Loop
    {
        for (u32 i = 0; i < vkcontext->renderCommandCount; i++)
        {
            RenderCommand *rc = &vkcontext->renderCommands[i];

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkcontext->pipeLayout,
                                    0, 1, &rc->desc->set, 0, 0);

            vkCmdPushConstants(cmd, vkcontext->pipeLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushData), &rc->pushData);

            vkCmdDrawIndexed(cmd, 6, rc->instanceCount, 0, 0, 0);
        }

        // Reset the Render Commands for next Frame
        vkcontext->renderCommandCount = 0;
    }

    vkCmdEndRenderPass(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    // This call will signal the Fence when the GPU Work is done
    VkSubmitInfo submitInfo = submit_info(&cmd);
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.pSignalSemaphores = &vkcontext->submitSemaphore;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &vkcontext->aquireSemaphore;
    submitInfo.waitSemaphoreCount = 1;
    VK_CHECK(vkQueueSubmit(vkcontext->graphicsQueue, 1, &submitInfo, vkcontext->imgAvailableFence));

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pSwapchains = &vkcontext->swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pImageIndices = &imgIdx;
    presentInfo.pWaitSemaphores = &vkcontext->submitSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    vkQueuePresentKHR(vkcontext->graphicsQueue, &presentInfo);

    return true;
}