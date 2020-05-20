# PROJECT_SOURCE_DIR should always be <ashuffle src>/subprojects/absl,
# so ../.. to get back to the true root.
get_filename_component(PROJECT_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../.. ABSOLUTE)
