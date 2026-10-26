project('arhi', ['cpp', 'c'], default_options: ['cpp_std=c++20'])

arhi_deps = []

arhi_sources = [
    'Source/IRenderBackend.hpp',
    'Source/Types.hpp',
]

if(get_option('build_vulkan'))
    
    arhi_sources += [
        'Source/Vulkan/VulkanBackend.cpp',
        'Source/Vulkan/VulkanBackend.hpp',
        'Source/Vulkan/VulkanTypes.hpp'
    ]

    arhi_deps += dependency('vulkan', required: true)
    add_project_arguments('-DARHI_BUILD_VULKAN', language: 'cpp')

endif

if(get_option('platform_backend') == 'glfw')
    add_project_arguments('-DARHI_BACKEND_GLFW', language: 'cpp')
    arhi_deps += dependency('glfw3', static: true)
endif

if(get_option('platform_backend') == 'sdl3')
    add_project_arguments('-DARHI_BACKEND_SDL3', language: 'cpp')
    arhi_deps += dependency('sdl3', static: true)
endif

arhi_includes = [
    'Source/'
]

static_library('arhi', arhi_sources, dependencies: arhi_deps, include_directories: arhi_includes)