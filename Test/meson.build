arhi_test_sources = [
    'main.cpp'
]

arhi_test_deps = [arhi]
arhi_test_deps += dependency('glfw3', static: true)

arhi_test_includes = [
    '../Source',
    './'
]

executable('arhi-test', arhi_test_sources, dependencies: arhi_test_deps, include_directories: arhi_test_includes)