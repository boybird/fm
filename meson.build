project('fm', 'c', version: '0.1', default_options: ['warning_level=2', 'c_std=c11'])

# Define compiler
cc = meson.get_compiler('c')

# Include directory
incdir = include_directories('include')

# Source files
sources = files(
  'src/db_manager.c',
  'src/error_handler.c',
  'src/file_manager.c',
  'src/main.c',
)

# Dependencies
sqlite3_dep = dependency('sqlite3', required: true)
crypto_dep = dependency('libcrypto', required: true)
json_dep = dependency('json-c', required: true)

# Build the executable
executable(
  'fm',
  sources: sources,
  include_directories: incdir,
  dependencies: [sqlite3_dep, crypto_dep, json_dep],
  install: false,
)
