# Set HOST and buildprefix
HOST ?= x86_64-pc-linux-gnu
buildprefix=$(HOST)

# Define paths based on HOST and buildprefix
download_dir=$(CURDIR)/../work/download
extracted_dir=$(CURDIR)/../work/extracted
install_dir=$(CURDIR)/../$(buildprefix)

# Define variables for OpenCL-Headers
package=OpenCL-Headers
version=2024.05.08
$(package)_version=$(version)
$(package)_download_path=https://github.com/KhronosGroup/OpenCL-Headers/archive/refs/tags/v$(version).tar.gz
$(package)_file_name=$(package)-$(version).tar.gz
$(package)_sha256_hash=3c3dd236d35f4960028f4f58ce8d963fb63f3d50251d1e9854b76f1caab9a309
$(package)_extracted_subdir=$(extracted_dir)/$(package)

# Define variables for OpenCL-ICD-Loader
package_icd=Opencl-ICD-Loader
version_icd=2024.05.08
$(package_icd)_version=$(version_icd)
$(package_icd)_download_path=https://github.com/KhronosGroup/OpenCL-ICD-Loader/archive/refs/tags/v$(version_icd).tar.gz
$(package_icd)_file_name=$(package_icd)-$(version_icd).tar.gz
$(package_icd)_sha256_hash=eb2c9fde125ffc58f418d62ad83131ba686cccedcb390cc7e6bb81cc5ef2bd4f
$(package_icd)_extracted_subdir=$(extracted_dir)/$(package_icd)

# Download, verify, and extract OpenCL-Headers
.PHONY: $(package)_download
$(package)_download:
	mkdir -p $(download_dir)
	echo "Downloading $(package) from $($(package)_download_path)"
	wget -O $(download_dir)/$($(package)_file_name) $($(package)_download_path)
	echo "Verifying checksum"
	echo "$($(package)_sha256_hash) $(download_dir)/$($(package)_file_name)" | sha256sum -c || (echo "Checksum failed" && exit 1)
	echo "Extracting to $($(package)_extracted_subdir)"
	mkdir -p $($(package)_extracted_subdir)
	tar -xzf $(download_dir)/$($(package)_file_name) -C $($(package)_extracted_subdir) --strip-components=1

# Download, verify, and extract OpenCL-ICD-Loader
.PHONY: $(package_icd)_download
$(package_icd)_download:
	mkdir -p $(download_dir)
	echo "Downloading $(package_icd) from $($(package_icd)_download_path)"
	wget -O $(download_dir)/$($(package_icd)_file_name) $($(package_icd)_download_path)
	echo "Verifying checksum"
	echo "$($(package_icd)_sha256_hash) $(download_dir)/$($(package_icd)_file_name)" | sha256sum -c || (echo "Checksum failed" && exit 1)
	echo "Extracting to $($(package_icd)_extracted_subdir)"
	mkdir -p $($(package_icd)_extracted_subdir)
	tar -xzf $(download_dir)/$($(package_icd)_file_name) -C $($(package_icd)_extracted_subdir) --strip-components=1

# Build and install OpenCL-Headers
.PHONY: $(package)_build
$(package)_build: $(package)_download
	cd $($(package)_extracted_subdir) && \
	cmake -DCMAKE_INSTALL_PREFIX=$(install_dir) . && \
	make && make install

# Build and install OpenCL-ICD-Loader
.PHONY: $(package_icd)_build
$(package_icd)_build: $(package_icd)_download $(package)_build
	cd $($(package_icd)_extracted_subdir) && \
	cmake -DOpenCL_HEADERS_DIR=$(install_dir)/include \
          -DCMAKE_INSTALL_PREFIX=$(install_dir) . && \
	make && make install

# Clean OpenCL-Headers build directory
.PHONY: $(package)_clean
$(package)_clean:
	rm -rf $($(package)_extracted_subdir)
	rm -f $(download_dir)/$($(package)_file_name)

# Clean OpenCL-ICD-Loader build directory
.PHONY: $(package_icd)_clean
$(package_icd)_clean:
	rm -rf $($(package_icd)_extracted_subdir)
	rm -f $(download_dir)/$($(package_icd)_file_name)

# Overall build target for both packages
.PHONY: all
all: $(package)_build $(package_icd)_build
