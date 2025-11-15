sfmlGraphic = dependency('sfml-graphics', required: false)
sfmlWindow  = dependency('sfml-window', required: false)

if sfmlGraphic.found() and sfmlWindow.found()
    sfmlEmulator = executable('sfml_emulator', 'sfmlEmulator.cpp',
        link_with: [emulator_native],
        dependencies: [sfmlGraphic, sfmlWindow, thread],
        include_directories: [emulator_include],
        cpp_args: cpp_args,
        install:true)
endif

termEmulator = executable('term_emulator', 'termEmulator.cpp',
    link_with: [emulator_native],
    include_directories: [emulator_include],
    cpp_args: cpp_args,
    install:true)

