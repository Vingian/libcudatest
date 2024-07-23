/*
 * gcc -m64 libcudatest.c -o libcudatest -l vulkan
 *
 * Fail with code -3(VK_ERROR_INITIALIZATION_FAILED):
 *  ./libcudatest --alloc
 * Create the device normally:
 *  ./libcudatest
 *
 * Tested on 555.58.02
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <vulkan/vulkan.h>

#ifndef __x86_64__
#error "Wrong architecture..."
#endif

#define _countof(a) (sizeof(a)/sizeof(*(a)))

VkBool32 isExtensionSupported(VkExtensionProperties *exts, uint32_t extsCount, const char *ext)
{
	if(!exts || !ext) return VK_FALSE;
	for(uint32_t i = 0; i < extsCount; i++)
		if(strcmp(exts[i].extensionName, ext) == 0) return VK_TRUE;
	return VK_FALSE;
}

VkPhysicalDevice CompatibleDevice(VkInstance instance, const char **enabledExts, uint32_t enabledExtsCount)
{
	VkPhysicalDevice r = VK_NULL_HANDLE;

	if(!enabledExts || !enabledExtsCount) return r;

	uint32_t devicesCount;
	if(vkEnumeratePhysicalDevices(instance, &devicesCount, NULL) != VK_SUCCESS || devicesCount == 0) return r;
	VkPhysicalDevice *devices = (VkPhysicalDevice*)malloc(devicesCount * sizeof(VkPhysicalDevice));
	vkEnumeratePhysicalDevices(instance, &devicesCount, devices);

	for(uint32_t i = 0, j; i < devicesCount; i++) {
		uint32_t extsCount;
		if(vkEnumerateDeviceExtensionProperties(devices[i], NULL, &extsCount, NULL) == VK_SUCCESS && extsCount > 0) {
			VkExtensionProperties *exts = (VkExtensionProperties*)malloc(extsCount * sizeof(VkExtensionProperties));
			vkEnumerateDeviceExtensionProperties(devices[i], NULL, &extsCount, exts);
			for(j = 0; j < enabledExtsCount; j++)
				if(!isExtensionSupported(exts, extsCount, enabledExts[j])) break;
			if(j == enabledExtsCount) {
				r = devices[i];
				break;
			}
		}
	}

	free(devices);

	return r;
}

int main(int argc, char **argv) {
	int r = 1, alloc = 0;

	for(int i = 1; i < argc; i++)
		if(strcasecmp(argv[i], "--alloc") == 0) alloc = 1;

	VkInstance instance;
	VkInstanceCreateInfo instanceInfo = {};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	VkResult result = vkCreateInstance(&instanceInfo, NULL, &instance);
	if(result != VK_SUCCESS) {
		fprintf(stderr, "vkCreateInstance failed: %i\n", result);
		return r;
	} else printf("Instance created successfully!\n");

	const char *EnabledExtensions[] = {VK_NVX_BINARY_IMPORT_EXTENSION_NAME};

	VkPhysicalDevice physicalDevice = CompatibleDevice(instance, EnabledExtensions, _countof(EnabledExtensions));
	if(physicalDevice != VK_NULL_HANDLE) {
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		printf("Compatible device found: %s\n", properties.deviceName);

		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.enabledExtensionCount = _countof(EnabledExtensions);
		deviceCreateInfo.ppEnabledExtensionNames = EnabledExtensions;

		size_t s = (size_t)0xFFF8000000 - (size_t)0x0200000000;
		void *m = alloc ? mmap((void*)0x0200000000, s, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) : MAP_FAILED;
		if(m != MAP_FAILED) printf("Mapped: %p(%p)\n", m, s);

		VkDevice device;
		result = vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device);
		if(result == VK_SUCCESS) {
			printf("Device created successfully!\n");
			vkDestroyDevice(device, NULL);
			r = 0;
		} else fprintf(stderr, "vkCreateDevice failed: %i\n", result);

		munmap(m, s);
	} else fprintf(stderr, "No compatible devices found.\n");

	vkDestroyInstance(instance, NULL);

	return r;
}