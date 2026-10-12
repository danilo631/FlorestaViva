#include <vulkan/vulkan.h>
#include <android/asset_manager.h>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

// True Vulkan compute pass, executed once when an Android GLES surface starts.
// Vulkan writes a host-visible buffer; OpenGL ES consumes its bytes as an RGBA
// texture. There is no claim that Vulkan renders the visible forest scene.
bool bakeVulkanWindField(AAssetManager* manager,std::vector<uint8_t>& rgba,
                         std::string& gpuName){
    if(!manager)return false;
    AAsset* asset=AAssetManager_open(manager,"vulkan/wind.comp.spv",AASSET_MODE_BUFFER);
    if(!asset)return false;
    size_t bytes=size_t(AAsset_getLength(asset));
    std::vector<uint32_t> code((bytes+3)/4);
    const int read=AAsset_read(asset,code.data(),bytes);
    AAsset_close(asset);
    if(read<0 || size_t(read)!=bytes || (bytes%4)!=0)return false;

    VkInstance instance=VK_NULL_HANDLE;
    VkDevice device=VK_NULL_HANDLE;
    VkBuffer buffer=VK_NULL_HANDLE;
    VkDeviceMemory memory=VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout=VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool=VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout=VK_NULL_HANDLE;
    VkPipeline pipeline=VK_NULL_HANDLE;
    VkShaderModule shader=VK_NULL_HANDLE;
    VkCommandPool commandPool=VK_NULL_HANDLE;
    VkFence fence=VK_NULL_HANDLE;
    bool success=false;

    do{
        VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        ai.pApplicationName="FlorestaViva";
        ai.apiVersion=VK_API_VERSION_1_0;
        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ci.pApplicationInfo=&ai;
        if(vkCreateInstance(&ci,nullptr,&instance)!=VK_SUCCESS)break;
        uint32_t physicalCount=0;
        if(vkEnumeratePhysicalDevices(instance,&physicalCount,nullptr)!=VK_SUCCESS
            || !physicalCount)break;
        std::vector<VkPhysicalDevice> physicals(physicalCount);
        if(vkEnumeratePhysicalDevices(instance,&physicalCount,physicals.data())!=VK_SUCCESS)break;
        VkPhysicalDevice physical=VK_NULL_HANDLE;
        uint32_t queueIndex=0;
        for(auto d:physicals){
            uint32_t count=0;
            vkGetPhysicalDeviceQueueFamilyProperties(d,&count,nullptr);
            std::vector<VkQueueFamilyProperties> properties(count);
            vkGetPhysicalDeviceQueueFamilyProperties(d,&count,properties.data());
            for(uint32_t q=0;q<count;q++){
                if(properties[q].queueFlags&VK_QUEUE_COMPUTE_BIT){
                    physical=d;queueIndex=q;break;
                }
            }
            if(physical!=VK_NULL_HANDLE)break;
        }
        if(physical==VK_NULL_HANDLE)break;
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physical,&properties);
        gpuName=properties.deviceName;
        const float priority=1.f;
        VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qi.queueFamilyIndex=queueIndex;qi.queueCount=1;qi.pQueuePriorities=&priority;
        VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;
        if(vkCreateDevice(physical,&di,nullptr,&device)!=VK_SUCCESS)break;
        VkQueue queue=VK_NULL_HANDLE;
        vkGetDeviceQueue(device,queueIndex,0,&queue);
        constexpr VkDeviceSize length=128u*128u*4u;
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size=length;
        bi.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        if(vkCreateBuffer(device,&bi,nullptr,&buffer)!=VK_SUCCESS)break;
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device,buffer,&requirements);
        VkPhysicalDeviceMemoryProperties memoryProperties{};
        vkGetPhysicalDeviceMemoryProperties(physical,&memoryProperties);
        uint32_t memoryIndex=UINT32_MAX;
        bool coherent=false;
        for(uint32_t i=0;i<memoryProperties.memoryTypeCount;i++){
            if(!(requirements.memoryTypeBits&(1u<<i)))continue;
            VkMemoryPropertyFlags flags=memoryProperties.memoryTypes[i].propertyFlags;
            if((flags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)==0)continue;
            memoryIndex=i;
            if(flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT){coherent=true;break;}
        }
        if(memoryIndex==UINT32_MAX)break;
        VkMemoryAllocateInfo memoryInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        memoryInfo.allocationSize=requirements.size;
        memoryInfo.memoryTypeIndex=memoryIndex;
        if(vkAllocateMemory(device,&memoryInfo,nullptr,&memory)!=VK_SUCCESS)break;
        if(vkBindBufferMemory(device,buffer,memory,0)!=VK_SUCCESS)break;

        VkDescriptorSetLayoutBinding binding{};
        binding.binding=0;binding.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding.descriptorCount=1;binding.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
        VkDescriptorSetLayoutCreateInfo dli{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dli.bindingCount=1;dli.pBindings=&binding;
        if(vkCreateDescriptorSetLayout(device,&dli,nullptr,&descriptorLayout)!=VK_SUCCESS)break;
        VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1};
        VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        dpi.maxSets=1;dpi.poolSizeCount=1;dpi.pPoolSizes=&poolSize;
        if(vkCreateDescriptorPool(device,&dpi,nullptr,&descriptorPool)!=VK_SUCCESS)break;
        VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dai.descriptorPool=descriptorPool;dai.descriptorSetCount=1;
        dai.pSetLayouts=&descriptorLayout;
        VkDescriptorSet set=VK_NULL_HANDLE;
        if(vkAllocateDescriptorSets(device,&dai,&set)!=VK_SUCCESS)break;
        VkDescriptorBufferInfo bufferInfo{buffer,0,length};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet=set;write.dstBinding=0;write.descriptorCount=1;
        write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;write.pBufferInfo=&bufferInfo;
        vkUpdateDescriptorSets(device,1,&write,0,nullptr);

        VkShaderModuleCreateInfo si{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        si.codeSize=bytes;si.pCode=code.data();
        if(vkCreateShaderModule(device,&si,nullptr,&shader)!=VK_SUCCESS)break;
        VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pli.setLayoutCount=1;pli.pSetLayouts=&descriptorLayout;
        if(vkCreatePipelineLayout(device,&pli,nullptr,&pipelineLayout)!=VK_SUCCESS)break;
        VkComputePipelineCreateInfo pi{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pi.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pi.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;
        pi.stage.module=shader;pi.stage.pName="main";pi.layout=pipelineLayout;
        if(vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&pi,nullptr,&pipeline)!=VK_SUCCESS)break;

        VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cpi.queueFamilyIndex=queueIndex;
        if(vkCreateCommandPool(device,&cpi,nullptr,&commandPool)!=VK_SUCCESS)break;
        VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cai.commandPool=commandPool;cai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cai.commandBufferCount=1;
        VkCommandBuffer commands=VK_NULL_HANDLE;
        if(vkAllocateCommandBuffers(device,&cai,&commands)!=VK_SUCCESS)break;
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if(vkBeginCommandBuffer(commands,&begin)!=VK_SUCCESS)break;
        vkCmdBindPipeline(commands,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
        vkCmdBindDescriptorSets(commands,VK_PIPELINE_BIND_POINT_COMPUTE,
                                pipelineLayout,0,1,&set,0,nullptr);
        vkCmdDispatch(commands,16,16,1);
        if(vkEndCommandBuffer(commands)!=VK_SUCCESS)break;
        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if(vkCreateFence(device,&fci,nullptr,&fence)!=VK_SUCCESS)break;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount=1;submit.pCommandBuffers=&commands;
        if(vkQueueSubmit(queue,1,&submit,fence)!=VK_SUCCESS)break;
        if(vkWaitForFences(device,1,&fence,VK_TRUE,2500000000ull)!=VK_SUCCESS)break;
        void* mapped=nullptr;
        if(vkMapMemory(device,memory,0,requirements.size,0,&mapped)!=VK_SUCCESS)break;
        if(!coherent){
            VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
            range.memory=memory;range.offset=0;range.size=VK_WHOLE_SIZE;
            if(vkInvalidateMappedMemoryRanges(device,1,&range)!=VK_SUCCESS){
                vkUnmapMemory(device,memory);break;
            }
        }
        rgba.resize(size_t(length));
        std::memcpy(rgba.data(),mapped,size_t(length));
        vkUnmapMemory(device,memory);
        success=true;
    }while(false);
    if(device!=VK_NULL_HANDLE)vkDeviceWaitIdle(device);
    if(fence)vkDestroyFence(device,fence,nullptr);
    if(commandPool)vkDestroyCommandPool(device,commandPool,nullptr);
    if(pipeline)vkDestroyPipeline(device,pipeline,nullptr);
    if(pipelineLayout)vkDestroyPipelineLayout(device,pipelineLayout,nullptr);
    if(shader)vkDestroyShaderModule(device,shader,nullptr);
    if(descriptorPool)vkDestroyDescriptorPool(device,descriptorPool,nullptr);
    if(descriptorLayout)vkDestroyDescriptorSetLayout(device,descriptorLayout,nullptr);
    if(buffer)vkDestroyBuffer(device,buffer,nullptr);
    if(memory)vkFreeMemory(device,memory,nullptr);
    if(device)vkDestroyDevice(device,nullptr);
    if(instance)vkDestroyInstance(instance,nullptr);
    return success;
}
