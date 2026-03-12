# custom-apps/CMakeLists.txt

# Add application subdirectories here, e.g.:
# add_subdirectory(my-app)

nuttx_add_subdirectory()
nuttx_generate_kconfig(MENUDESC "Custom Applications")
