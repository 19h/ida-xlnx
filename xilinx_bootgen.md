# Bootgen User Guide (UG1283)

Confidential - Copyright © Fluid Topics

## Table of Contents
* Zynq 7000 SoC Boot and Configuration
  * Zynq 7000 SoC Boot Image Layout
  * Zynq 7000 SoC Boot Header
  * Zynq 7000 SoC Register Initialization Table
  * Zynq 7000 SoC Image Header Table
  * Zynq 7000 SoC Image Header
  * Zynq 7000 SoC Partition Header
  * Zynq 7000 SoC Authentication Certificate
  * Zynq 7000 SoC Boot Image Block Diagram
* Zynq UltraScale+ MPSoC Boot and Configuration
  * Zynq UltraScale+ MPSoC Boot Image
  * Zynq UltraScale+ MPSoC Boot Header
  * Zynq UltraScale+ MPSoC Register Initialization Table
  * Zynq UltraScale+ MPSoC PUF Helper Data
  * Zynq UltraScale+ MPSoC Image Header Table
  * Zynq UltraScale+ MPSoC Image Header
  * Zynq UltraScale+ MPSoC Partition Header
  * Zynq UltraScale+ MPSoC Authentication Certificates
  * Zynq UltraScale+ MPSoC Secure Header
  * Zynq UltraScale+ MPSoC Boot Image Block Diagram
* Versal Adaptive SoC Boot Image Format
  * Versal Adaptive SoC Boot Header
  * Versal Adaptive SoC Boot Header Attributes
  * Versal Adaptive SoC Image Header Table
  * Versal Adaptive SoC Image Header
  * Versal Adaptive SoC Partition Header
  * Versal Adaptive SoC Authentication Certificates
* Spartan UltraScale+ Boot Image Format
  * Spartan UltraScale+ Boot Header
  * Spartan UltraScale+ Boot Header Attributes
  * Spartan UltraScale+ Authentication Certificate
* Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Boot Image Format
  * Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Boot Header
  * Versal AI Edge Gen 2 and Versal Prime Gen 2 Boot Header Attributes
  * Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Image Header Table
  * Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Image Header
  * Versal AI Edge Gen 2 and Versal Prime Gen 2 Partition Header
  * Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Authentication Certificate
* Creating Boot Images
  * Boot Image Format (BIF)
  * BIF Syntax and Supported File Types
  * Attributes
  * Spartan UltraScale+ ID Code Based Update

---

## Zynq 7000 SoC Boot and Configuration

This section describes the boot and configuration sequence for Zynq 7000 SoC devices. See the *Zynq 7000 SoC Technical Reference Manual* (UG585) for more details on the available first stage boot loader (FSBL) structures.

### BootROM on Zynq 7000 SoC

The BootROM is the first software to run in the application processing unit (APU). BootROM executes on the first Cortex® processor, A9-0, while the second processor, Cortex, A9-1, executes the wait for event (WFE) instruction. The main tasks of the BootROM are to configure the system, copy the FSBL from the boot device to the on-chip memory (OCM), and then branch the code execution to the OCM.

Optionally, you can execute the FSBL directly from a Quad-SPI or NOR device in a non-secure environment. The master boot device holds one or more boot images. A boot image is made up of the boot header and the first stage boot loader (FSBL). Additionally, a boot image can have programmable logic (PL), a second stage boot loader (SSBL), and an embedded operating system and applications; however, these are not accessed by the BootROM. The BootROM execution flow is affected by the boot mode pin strap settings, the boot header, and what it discovers about the system. The BootROM can execute in a secure environment with encrypted FSBL, or a non-secure environment. The supported boot modes are:

* JTAG mode is primarily used for development and debug.
* NAND, parallel NOR, Serial NOR (Quad-SPI), and secure digital (SD) flash memories are used for booting the device.

The *Zynq 7000 SoC Technical Reference Manual* (UG585) provides the details of these boot modes. See Answer Record 52538 for answers to common boot and configuration questions.

### Zynq 7000 SoC Boot Image Layout

The following is a diagram of the components that can be included in an AMD Zynq™ 7000 SoC boot image.

**Figure: Boot Header**

* Boot Header
* Register Initialization Table
* Image Header Table
* Image Header 1 | Image Header 2 | ... | Image Header n
* Partition Header 1 | Partition Header 2 | ... | Partition Header n
* Header Authentication Certificate (Optional)
* Partition 1 (FSBL) | AC (Optional)
* Partition 2 | AC (Optional)
* ...
* Partition n | AC (Optional)

### Zynq 7000 SoC Boot Header

Additionally, the Boot Header contains a Zynq 7000 SoC Register Initialization Table. BootROM uses the boot header to find the location and length of FSBL and other details to initialize the system before handing off the control to FSBL.

Bootgen attaches a boot header at the beginning of a boot image. The boot header table is a structure that contains information related to booting the primary bootloader, such as the FSBL. There is only one such structure in the entire boot image. This table is parsed by BootROM to determine where the FSBL is stored in flash and where it needs to be loaded in OCM. Some encryption and authentication related parameters are also stored in here.

The additional boot image components are:
* Zynq 7000 SoC Image Header Table
* Zynq 7000 SoC Image Header
* Zynq 7000 SoC Partition Header
* Zynq 7000 SoC Authentication Certificate

The following table provides the address offsets, parameters, and descriptions for the AMD Zynq™ 7000 SoC Boot Header.

**Table: Zynq 7000 SoC Boot Header**

| Address Offset | Parameter | Description |
| :--- | :--- | :--- |
| 0x00-0x1F | Arm® Vector table | Filled with dummy vector table by Bootgen (Arm Op code `0xEAFFFFFE`, which is a branch-to-self infinite loop intended to catch uninitialized vectors. |
| 0x20 | Width Detection Word | This is required to identify the QSPI flash in single/dual stacked or dual parallel mode. `0xAA995566` in little endian format. |
| 0x24 | Header Signature | Contains 4 bytes ‘X’,’N’,’L’,’X’ in byte order, which is `0x584c4e58` in little endian format. |
| 0x28 | Key Source | Location of encryption key within the device:<br>• `0x3A5C3C5A`: Encryption key in BBRAM.<br>• `0xA5C3C5A3`: Encryption key in eFUSE.<br>• `0x00000000`: Not Encrypted. |
| 0x2C | Header Version | `0x01010000` |
| 0x30 | Source Offset | Location of FSBL (bootloader) in this image file. |
| 0x34 | FSBL Image Length | Length of the FSBL, after decryption. |
| 0x38 | FSBL Load Address (RAM) | Destination RAM address to which to copy the FSBL. |
| 0x3C | FSBL Execution address (RAM) | Entry vector for FSBL execution. |
| 0x40 | Total FSBL Length | Total size of FSBL after encryption, including authentication certificate (if any) and padding. |
| 0x44 | QSPI Configuration Word | Hard coded to `0x00000001`. |
| 0x48 | Boot Header Checksum | Inverse of sum of words from offset `0x20` to `0x44` inclusive as per standard algorithm. The words are assumed to be little endian. |
| 0x4c-0x97 | User Defined Fields | 76 bytes |
| 0x98 | Image Header Table Offset | Pointer to Image Header Table |
| 0x9C | Partition Header Table Offset | Pointer to Partition Header Table |

### Zynq 7000 SoC Register Initialization Table

The Register Initialization Table in Bootgen is a structure of 256 address-value pairs used to initialize PS registers for MIO multiplexer and flash clocks. For more information, see About Register Intialization Pairs and INT File Attributes.

**Table: Zynq 7000 SoC Register Initialization Table**

| Address Offset | Parameter | Description |
| :--- | :--- | :--- |
| 0xA0 to 0x89C | Register Initialization Pairs: `<address>:<value>:` | Address = `0xFFFFFFFF` means skip that register and ignore the value. All the unused register fields must be set to Address=`0xFFFFFFFF` and value = `0x0`. |

### Zynq 7000 SoC Image Header Table

Bootgen creates a boot image by extracting data from ELF files, bitstream, data files, and so forth. These files, from which the data is extracted, are referred to as images. Each image can have one or more partitions. The Image Header table is a structure containing information that is common across all these images, and information like the number of images, partitions present in the boot image, and the pointer to the other header tables. The following table provides the address offsets, parameters, and descriptions for the AMD Zynq™ 7000 SoC device.

**Table: Zynq 7000 SoC Image Header Table**

| Address Offset | Parameter | Description |
| :--- | :--- | :--- |
| 0x00 | Version | `0x01010000`: Only fields available are `0x0`, `0x4`, `0x8`, `0xC`, and a padding `0x01020000:0x10` field is added. |
| 0x04 | Count of Image Headers | Indicates the number of image headers. |
| 0x08 | First Partition Header Offset | Pointer to first partition header. (word offset) |
| 0x0C | First Image Header Offset | Pointer to first image header. (word offset) |
| 0x10 | Header Authentication Certificate Offset | Pointer to the authentication certificate header. (word offset) |
| 0x14 | Reserved | Defaults to `0xFFFFFFFF`. |

### Zynq 7000 SoC Image Header

The Image Header is an array of structures containing information related to each image, such as an ELF file, bitstream, data files, and so forth. Each image can have multiple partitions; for example, an ELF can have multiple loadable sections, each of which forms a partition in the boot image. The table also contains the information of number of partitions related to an image. The following table provides the address offsets, parameters, and descriptions for the AMD Zynq™ 7000 SoC device.

**Table: Zynq 7000 SoC Image Header**

| Address Offset | Parameter | Description |
| :--- | :--- | :--- |
| 0x00 | Next Image Header | Link to next Image Header. 0 if last Image Header (word offset). |
| 0x04 | Corresponding partition header | Link to first associated Partition Header (word offset). |
| 0x08 | Reserved | Always 0. |
| 0x0C | Partition Count Length | Number of partitions associated with this image. |
| 0x10 to N | Image Name | Packed in big endian order. To reconstruct the string, unpack 4 bytes at a time, reverse the order, and concatenate. For example, the string “FSBL10.ELF” is packed as `0x10`: `‘L’,’B’,’S’,’F’`, `0x14`: `’E’,’.’,’0’,’1’`, `0x18`: `’\0’,’\0’,’F’,’L’`. The packed image name is a multiple of 4 bytes. |
| N | String Terminator | `0x00000000` |
| N+4 | Reserved | Defaults to `0xFFFFFFFF` to 64 bytes boundary. |

### Zynq 7000 SoC Partition Header

The Partition Header is an array of structures containing information related to each partition. Each partition header table is parsed by the Boot Loader. The information such as the partition size, address in flash, load address in RAM, encrypted/signed, and so forth, are part of this table. There is one such structure for each partition including FSBL. The last structure in the table is marked by all NULL values (except the checksum). The following table shows the offsets, names, and notes regarding the AMD Zynq™ 7000 SoC Partition Header.

> ✎ **Note:** An ELF file with three loadable sections has one image header and three partition header tables.

**Table: Zynq 7000 SoC Partition Header**

| Offset | Name | Notes |
| :--- | :--- | :--- |
| 0x00 | Encrypted Partition length | Encrypted partition data length. |
| 0x04 | Unencrypted Partition length | Unencrypted data length. |
| 0x08 | Total partition word length (Includes Authentication Certificate.) See Zynq 7000 SoC Authentication Certificate. | The total partition word length comprises the encrypted information length with padding, the expansion length, and the authentication length. |
| 0x0C | Destination load address. | The RAM address into which this partition is to be loaded. |
| 0x10 | Destination execution address. | Entry point of this partition when executed. |
| 0x14 | Data word offset in Image | Position of the partition data relative to the start of the boot image. |
| 0x18 | Attribute Bits | See Zynq 7000 SoC Partition Attribute Bits |
| 0x1C | Section Count | Number of sections in a single partition. |
| 0x20 | Checksum Word Offset | Location of the corresponding checksum word in the boot image. |
| 0x24 | Image Header Word Offset | Location of the corresponding Image Header in the boot image. |
| 0x28 | Authentication Certification Word Offset | Location of the corresponding Authentication Certification in the boot image. |
| 0x2C-0x38 | Reserved | Reserved |
| 0x3C | Header Checksum | Inverse of sum of the previous words as per standard algorithm in the Partition Header. |

### Zynq 7000 SoC Partition Attribute Bits

The following table describes the Partition Attribute bits of the partition header table for an AMD Zynq™ 7000 SoC device.

**Table: Zynq 7000 SoC Partition Attribute Bits**

| Bit Field | Description | Notes |
| :--- | :--- | :--- |
| 31:18 | Reserved | Not used |
| 17:16 | Partition owner | • 0: FSBL<br>• 1: UBOOT<br>• 2 and 3: reserved |
| 15 | RSA signature present | • 0: No RSA authentication certificate<br>• 1: RSA authentication certificate |
| 14:12 | Checksum type | • 0: None<br>• 1: MD5<br>• 2-7: reserved |
| 11:8 | Reserved | Not used |
| 7:4 | Destination device | • 0: None<br>• 1: PS<br>• 2: PL<br>• 3: INT<br>• 4-15: Reserved |
| 3:2 | Reserved | Not used |
| 1:0 | Reserved | Not used |

### Zynq 7000 SoC Authentication Certificate

The Authentication Certificate is a structure that contains all the information related to the authentication of a partition. This structure has the public keys, all the signatures that BootROM/FSBL needs to verify. There is an Authentication Header in each Authentication Certificate, which gives information like the key sizes, algorithm used for signing, and so forth. The Authentication Certificate is appended to the actual partition, for which authentication is enabled. If authentication is enabled for any of the partitions, the header tables also needs authentication. Header Table Authentication Certificate is appended at end of the header tables content.

The AMD Zynq™ 7000 SoC uses an RSA-2048 authentication with a SHA-256 hashing algorithm, which means the primary and secondary key sizes are 2048-bit. Because SHA-256 is used as the secure hash algorithm, the FSBL, partition, and authentication certificates must be padded to a 512-bit boundary.

The format of the Authentication Certificate in an AMD Zynq™ 7000 SoC is as shown in the following table.

**Table: Zynq 7000 SoC Authentication Certificate**

| Authentication Certificate Bits | Description |
| :--- | :--- |
| 0x00 | Authentication Header = `0x0101000`. See Zynq 7000 SoC Authentication Certificate Header. |
| 0x04 | Certificate size |
| 0x08 | UDF (56 bytes) |
| 0x40 | PPK Mod (256 bytes) |
| 0x140 | PPK Mod Ext (256 bytes) |
| 0x240 | PPK Exponent |
| 0x244 | PPK Pad (60 bytes) |
| 0x280 | SPK Mod (256 bytes) |
| 0x380 | SPK Mod Ext (256 bytes) |
| 0x480 | SPK Exponent (4 bytes) |
| 0x484 | SPK Pad (60 bytes) |
| 0x4C0 | SPK Signature = RSA-2048 (PSK, Padding \|\| SHA-256 (SPK)) |
| 0x5C0 | FSBL Partition Signature = RSA-2048 (SSK, SHA256 (Boot Header \|\| FSBL partition)) |
| 0x5C0 | Other Partition Signature = RSA-2048 (SSK, SHA-256 (Partition \|\| Padding \|\| Authentication Header \|\| PPK \|\| SPK \|\| SPK Signature)) |

### Zynq 7000 SoC Authentication Certificate Header

The following table describes the AMD Zynq™ 7000 SoC Authentication Certificate Header.

**Table: Zynq 7000 SoC Authentication Certificate Header**

| Bit Offset | Field Name | Description |
| :--- | :--- | :--- |
| 31:16 | Reserved | 0 |
| 15:14 | Authentication Certificate Format | 00: PKCS #1 v1.5 |
| 13:12 | Authentication Certificate Version | 00: Current AC |
| 11 | PPK Key Type | 0: Hash Key |
| 10:9 | PPK Key Source | 0: eFUSE |
| 8 | SPK Enable | 1: SPK Enable |
| 7:4 | Public Strength | 0:2048 |
| 3:2 | Hash Algorithm | 0: SHA256 |
| 1:0 | Public Algorithm | 0: Reserved<br>1: RSA<br>2: Reserved<br>3: Reserved |

### Zynq 7000 SoC Boot Image Block Diagram

The following is a diagram of the components that can be included in an AMD Zynq™ 7000 SoC boot image.

**Figure: Zynq 7000 SoC Boot Image Block Diagram**

*(Diagram showing Boot Header, Image Header Table, Image Headers, Partition Headers, Header AC, Bootloader (FSBL), Bootloader AC, Partition 1, Partition 1 AC, Partition (n), Partition (n) AC)*

---

## Zynq UltraScale+ MPSoC Boot and Configuration

### Introduction

AMD Zynq™ UltraScale+™ MPSoC supports the ability to boot from different devices such as a QSPI flash, an SD card, USB device firmware upgrade (DFU) host, and the NAND flash drive. This chapter details the boot-up process using different booting devices in both secure and non-secure modes. The boot-up process is managed and carried out by the Platform Management Unit (PMU) and Configuration Security Unit (CSU).

During initial boot, the following steps occur:

*   The PMU is brought out of reset by the power on reset (POR).
*   The PMU executes code from PMU ROM.
*   The PMU initializes the SYSMON and required PLLs for the boot, clears the low power and full power domains, and releases the CSU reset.

After the PMU releases the CSU, CSU does the following:

*   Checks to determine if authentication is required by the FSBL or the user application.
*   Performs an authentication check and proceeds only if the authentication check passes. Then checks the image for any encrypted partitions.
*   If the CSU detects partitions that are encrypted, the CSU performs decryption and initializes OCM, determines boot mode settings, performs the FSBL load and an optional PMU firmware load.
*   After execution of CSU ROM code, it hands off control to FSBL. FSBL uses PCAP interface to program the PL with bitstream.

FSBL then takes the responsibility of the system. The *Zynq UltraScale+ Device Technical Reference Manual* (UG1085) provides details on CSU and PMU. For specific information on CSU, see "Configuration Security Unit" in the *Zynq UltraScale+ MPSoC: Software Developers Guide* (UG1137).

### Zynq UltraScale+ MPSoC Boot Image

The following figure shows the AMD Zynq™ UltraScale+™ MPSoC boot image.

**Figure: Zynq UltraScale+ MPSoC Boot Image**

*   Boot Header
*   Register Initialization Table
*   PUF Helper Data (Optional)
*   Image Header Table
*   Image Header 1 | Image Header 2 | ... | Image Header n
*   Partition Header 1 | Partition Header 2 | ... | Partition Header n
*   Header Authentication Certificate (Optional)
*   Partition 1 (FSBL) | PMU FW (Optional) | AC (Optional)
*   Partition 2 | AC (Optional)
*   ...
*   Partition n | AC (Optional)

### Zynq UltraScale+ MPSoC Boot Header

#### About the Boot Header

Bootgen attaches a boot header at the starting of any boot image. The boot header table is a structure that contains information related to booting of primary bootloader, such as the FSBL. There is only one such structure in entire boot image. This table is parsed by BootROM to get the information of where FSBL is stored in flash and where it needs to be loaded in OCM. Some encryption and authentication related parameters are also stored in here. The boot image components are:

*   Zynq UltraScale+ MPSoC Boot Header, which also has the Zynq UltraScale+ MPSoC Boot Header Attribute Bits.
*   Zynq UltraScale+ MPSoC Register Initialization Table
*   Zynq UltraScale+ MPSoC PUF Helper Data
*   Zynq UltraScale+ MPSoC Image Header Table
*   Zynq UltraScale+ MPSoC Image Header
*   Zynq UltraScale+ MPSoC Authentication Certificates
*   Zynq UltraScale+ MPSoC Partition Header

BootROM uses the boot header to find the location and length of FSBL and other details to initialize the system before handing off the control to FSBL. The following table provides the address offsets, parameters, and descriptions for the Zynq UltraScale+ MPSoC device.

**Table: Zynq UltraScale+ MPSoC Device Boot Header**

| Address Offset | Parameter | Description |
| :--- | :--- | :--- |
| 0X00-0x1F | Arm® vector table | XIP ELF vector table:<br>• `0xEAFFFFFE`: for Cortex®-R5F and Cortex A53 (32-bit)<br>• `0x14000000`: for Cortex A53 (64-bit) |
| 0x20 | Width Detection Word | This field is used for QSPI width detection. `0xAA995566` in little endian format. |
| 0x24 | Header Signature | Contains 4 bytes ‘X’, ’N’, ’L’, ’X’ in byte order, which is `0x584c4e58` in little endian format. |
| 0x28 | Key Source | • `0x00000000` (Un-encrypted)<br>• `0xA5C3C5A5` (Black key stored in eFuse)<br>• `0xA5C3C5A7` (Obfuscated key stored in eFuse)<br>• `0x3A5C3C5A` (Red key stored in BBRAM)<br>• `0xA5C3C5A3` (eFUSE RED key stored in eFUSE)<br>• `0xA35C7CA5` (Obfuscated key stored in Boot Header)<br>• `0xA3A5C3C5` (USER key stored in Boot Header)<br>• `0xA35C7C53` (Black key stored in Boot Header) |
| 0x2C | FSBL Execution address (RAM) | FSBL execution address in OCM or XIP base address. |
| 0x30 | Source Offset | If no PMUFW, then it is the start offset of FSBL. If PMUFW, then start of PMUFW. |
| 0x34 | PMU Image Length | PMU firmware original image length in bytes. (0-128 KB).<br>• If size > 0, PMUFW is prefixed to FSBL.<br>• If size = 0, no PMUFW image. |
| 0x38 | Total PMU FW Length | Total PMUFW image length in bytes.(PMUFW length + encryption overhead) |
| 0x3C | FSBL Image Length | Original FSBL image length in bytes. (0-250 KB). If 0, XIP boot image is assumed. |
| 0x40 | Total FSBL Length | FSBL image length + Encryption overhead of FSBL image + Authentication Certificate, + 64 byte alignment + Hash size (integrity check). |
| 0x44 | FSBL Image Attributes | See Bit Attributes. |
| 0x48 | Boot Header Checksum | Inverse of sum of words from offset `0x20` to `0x44` inclusive as per standard algorithm. The words are assumed to be little endian. |
| 0x4C-0x68 | Obfuscated/Black Key Storage | Stores the Obfuscated key or Black key. |
| 0x6C | Shutter Value | 32-bit `PUF_SHUT` register value to configure PUF for shutter offset time and shutter open time. |
| 0x70 -0x94 | User-Defined Fields (UDF) | 40 bytes. |
| 0x98 | Image Header Table Offset | Pointer to Image Header Table. |
| 0x9C | Partition Header Table Offset | Pointer to Partition Header. |
| 0xA0-0xA8 | Secure Header IV | IV for secure header of bootloader partition. |
| 0xAC-0xB4 | Obfuscated/Black Key IV | IV for Obfuscated or Black key. |

#### Zynq UltraScale+ MPSoC Boot Header Attribute Bits

**Table: Zynq UltraScale+ MPSoC Boot Header Attribute Bits**

| Field Name | Bit Offset | Width | Default | Description |
| :--- | :--- | :--- | :--- | :--- |
| Reserved | 31:16 | 16 | 0x0 | Reserved. Must be 0. |
| BHDR RSA | 15:14 | 2 | 0x0 | • 0x3: RSA Authentication of the boot image is done, excluding verification of PPK hash and SPK ID.<br>• All others: RSA Authentication is decided based on eFuse RSA bits. |
| Reserved | 13:12 | 2 | 0x0 | NA |
| CPU Select | 11:10 | 2 | 0x0 | • 0x0: R5 Single<br>• 0x1: A53 Single 32-bit<br>• 0x2: A53 Single 64-bit<br>• 0x3: R5 Dual |
| Hashing Select | 9:8 | 2 | 0x0 | • 0x0, 0x1: No Integrity check<br>• 0x3: SHA3 for boot image (BI) integrity check |
| PUF-HD | 7:6 | 2 | 0x0 | • 0x3: PUF HD is part of boot header.<br>• All other: PUF HD is in eFuse |
| Reserved | 5:0 | 6 | 0x0 | Reserved for future use. Must be 0. |

### Zynq UltraScale+ MPSoC Register Initialization Table

The Register Initialization Table in Bootgen is a structure of 256 address-value pairs used to initialize PS registers for MIO multiplexer and flash clocks. For more information, see Initialization Pairs and INT File Attribute.

**Table: Zynq UltraScale+ MPSoC Register Initialization Table**

| Address Offset | Parameter | Description |
| :--- | :--- | :--- |
| 0xB8 to 0x8B4 | Register Initialization Pairs: `<address>:<value>:` (2048 bytes) | If the Address is set to `0xFFFFFFFF`, that register is skipped and the value is ignored. All unused register fields must be set to Address=`0xFFFFFFFF` and value =`0x0`. |

### Zynq UltraScale+ MPSoC PUF Helper Data

The PUF uses helper data to re-create the original KEK value over the complete guaranteed operating temperature and voltage range over the life of the part. The helper data consists of a `<syndrome_value>`, an `<aux_value>`, and a `<chash_value>`. The helper data can either be stored in eFUSEs or in the boot image. See puf_file for more information. Also, see the section "PUF Helper Data" in the *Zynq UltraScale+ Device Technical Reference Manual* (UG1085).

**Table: Zynq UltraScale+ MPSoC PUF Helper Data**

| Address Offset | Parameter | Description |
| :--- | :--- | :--- |
| 0x8B8 to 0xEC0 | PUF Helper Data (1544 bytes) | Valid only when Boot Header Offset 0x44 (bits 7:6) == 0x3. If the PUF HD is not inserted then Boot Header size = 2048 bytes. If the PUF Header Data is inserted, then the Boot Header size = 3584 bytes. PUF HD size = Total size = 1536 bytes of PUFHD + 4 bytes of CHASH + 2 bytes of AUX + 1 byte alignment = 1544 byte. |

### Zynq UltraScale+ MPSoC Image Header Table

Bootgen creates a boot image by extracting data from ELF files, bitstream, data files, and so forth. These files, from which the data is extracted, are referred to as images. Each image can have one or more partitions. The Image Header table is a structure, containing information which is common across all these images, and information like; the number of images, partitions present in the boot image, and the pointer to the other header tables.

**Table: Zynq UltraScale+ MPSoC Device Image Header Table**

| Address Offset | Parameter | Description |
| :--- | :--- | :--- |
| 0x00 | Version | • `0x01010000`<br>• `0x01020000` - `0x10` field is added |
| 0x04 | Count of Image Header | Indicates the number of image headers. |
| 0x08 | First Partition Header Offset | Pointer to first partition header (word offset). |
| 0x0C | First Image Offset Header | Pointer to first image header (word offset). |
| 0x10 | Header Authentication Certificate | Pointer to header authentication certificate (word offset). |
| 0x14 | Secondary Boot Device | Options are:<br>• 0 - Same boot device<br>• 1 - QSPI-32<br>• 2 - QSPI-24<br>• 3 - NAND<br>• 4 - SD0<br>• 5 - SD1<br>• 6 - SDLS<br>• 7 - EMMC/ MMC<br>• 8 - USB<br>• 9 - ETHERNET<br>• 10 - PCIE<br>• 11 - SATA |
| 0x18- 0x38 | Padding | Reserved (`0x0`) |
| 0x3C | Checksum | Inverse of sum of all the previous words as per standard algorithm in the image header. |

### Zynq UltraScale+ MPSoC Image Header

#### About Image Headers

The Image Header is an array of structures containing information related to each image, such as an ELF file, bitstream, data files, and so forth. Each image can have multiple partitions, for example an ELF can have multiple loadable sections, each of which form a partition in the boot image. The table also contains the information of number of partitions related to an image. The following table provides the address offsets, parameters, and descriptions for the AMD Zynq™ UltraScale+™ MPSoC.

**Table: Zynq UltraScale+ MPSoC Device Image Header**

| Address Offset | Parameter | Description |
| :--- | :--- | :--- |
| 0x00 | Next image header offset | Link to next Image Header. 0 if last Image Header (word offset). |
| 0x04 | Corresponding partition header | Link to first associated Partition Header (word offset). |
| 0x08 | Reserved | Always 0. |
| 0x0C | Partition Count | Value of the actual partition count. |
| 0x10 - N | Image Name | Packed in big endian order. To reconstruct the string, unpack 4 bytes at a time, reverse the order, and concatenated. For example, the string “FSBL10.ELF” is packed as `0x10`: `‘L’,’B’,’S’,’F’`, `0x14`: `’E’,’.’,’0’,’1’`, `0x18`: `’\0’,’\0’,’F’,’L’` The packed image name is a multiple of 4 bytes. |
| varies | String Terminator | `0x00000` |
| varies | Padding | Defaults to `0xFFFFFFF` to 64 bytes boundary. |

### Zynq UltraScale+ MPSoC Partition Header

#### About the Partition Header

The Partition Header is an array of structures containing information related to each partition. Each partition header table is parsed by the Boot Loader. The information such as the partition size, address in flash, load address in RAM, encrypted/signed, and so forth, are part of this table. There is one such structure for each partition including FSBL. The last structure in the table is marked by all NULL values (except the checksum.) The following table shows the offsets, names, and notes regarding the AMD Zynq™ UltraScale+™ MPSoC.

**Table: Zynq UltraScale+ MPSoC Device Partition Header**

| Offset | Name | Notes |
| :--- | :--- | :--- |
| 0x0 | Encrypted Partition Data Word Length | Encrypted partition data length. |
| 0x04 | Un-encrypted Data Word Length | Unencrypted data length. |
| 0x08 | Total Partition Word Length (Includes Authentication Certificate. See Authentication Certificate. | The total encrypted + padding + expansion +authentication length. |
| 0x0C | Next Partition Header Offset | Location of next partition header (word offset). |
| 0x10 | Destination Execution AddressLO | The lower 32-bits of executable address of this partition after loading. |
| 0x14 | Destination Execution Address HI | The higher 32-bits of executable address of this partition after loading. |
| 0x18 | Destination Load Address LO | The lower 32-bits of RAM address into which this partition is to be loaded. |
| 0x1C | Destination Load Address HI | The higher 32-bits of RAM address into which this partition is to be loaded. |
| 0x20 | Actual Partition Word Offset | The position of the partition data relative to the start of the boot image. (word offset) |
| 0x24 | Attributes | See Zynq UltraScale+ MPSoC Partition Attribute Bits |
| 0x28 | Section Count | The number of sections associated with this partition. |
| 0x2C | Checksum Word Offset | The location of the checksum table in the boot image. (word offset) |
| 0x30 | Image Header Word Offset | The location of the corresponding image header in the boot image. (word offset) |
| 0x34 | AC Offset | The location of the corresponding Authentication Certificate in the boot image, if present (word offset) |
| 0x38 | Partition Number/ID | Partition ID. |
| 0x3C | Header Checksum | Inverse of sum of the previous words as per standard algorithm in the Partition Header. |

#### Zynq UltraScale+ MPSoC Partition Attribute Bits

The following table describes the Partition Attribute bits on the partition header table for the AMD Zynq™ UltraScale+™ MPSoC.

**Table: Zynq UltraScale+ MPSoC Device Partition Attribute Bits**

| Bit Offset | Field Name | Description |
| :--- | :--- | :--- |
| 31:24 | Reserved | |
| 23 | Vector Location | Location of exception vector.<br>• 0: LOVEC (default)<br>• 1: HIVEC |
| 22:20 | Reserved | |
| 19 | Early Handoff | Handoff immediately after loading:<br>• 0: No Early Handoff<br>• 1: Early Handoff Enabled |
| 18 | Endianness | • 0: Little Endian<br>• 1: Big Endian |
| 17:16 | Partition Owner | • 0: FSBL<br>• 1: U-Boot<br>• 2 and 3: Reserved |
| 15 | RSA Authentication Certificate present | • 0: No RSA Authentication Certificate<br>• 1: RSA Authentication Certificate |
| 14:12 | Checksum Type | • 0: None<br>• 1-2: Reserved<br>• 3: SHA3<br>• 4-7: Reserved |
| 11:8 | Destination CPU | • 0: None<br>• 1: A53-0<br>• 2: A53-1<br>• 3: A53-2<br>• 4: A53-3<br>• 5: R5-0<br>• 6: R5 -1<br>• 7 R5-lockstep<br>• 8: PMU<br>• 9-15: Reserved |
| 7 | Encryption Present | • 0: Not Encrypted<br>• 1: Encrypted |
| 6:4 | Destination Device | • 0: None<br>• 1: PS<br>• 2: PL<br>• 3-15: Reserved |
| 3 | A5X Exec State | • 0: AARCH64 (default)<br>• 1: AARCH32 |
| 2:1 | Exception Level | • 0: EL0<br>• 1: EL1<br>• 2: EL2<br>• 3: EL3 |
| 0 | Trustzone | • 0: Non-secure<br>• 1: Secure |

### Zynq UltraScale+ MPSoC Authentication Certificates

The Authentication Certificate is a structure that contains all the information related to the authentication of a partition. This structure has the public keys and the signatures that BootROM/FSBL needs to verify. There is an Authentication Header in each Authentication Certificate, which gives information like the key sizes, algorithm used for signing, and so forth. The Authentication Certificate is appended to the actual partition, for which authentication is enabled. If authentication is enabled for any of the partitions, the header tables also needs authentication. The Header Table Authentication Certificate is appended at end of the content to the header tables.

The AMD Zynq™ UltraScale+™ MPSoC uses RSA-4096 authentication, which means the primary and secondary key sizes are 4096-bit. The following table provides the format of the Authentication Certificate for the Zynq UltraScale+ MPSoC device.

**Table: Zynq UltraScale+ MPSoC Device Authentication Certificates**

| Authentication Certificate | |
| :--- | :--- |
| 0x00 | Authentication Header = `0x0101000`. See Zynq UltraScale+ MPSoC Authentication Certification Header. |
| 0x04 | SPK ID |
| 0x08 | UDF (56 bytes) |
| 0x40 | PPK Mod (512) |
| 0x240 | PPK Mod Ext (512) |
| 0x440 | PPK Exponent (4 bytes) |
| 0x444 | PPK Pad (60 bytes) |
| 0x480 | SPK Mod (512 bytes) |
| 0x680 | SPK Mod Ext (512 bytes) |
| 0x880 | SPK Exponent (4 bytes) |
| 0x884 | SPK Pad (60 bytes) |
| 0x8C0 | SPK Signature = RSA-4096 ( PSK, Padding \|\| SHA-384 (SPK + Authentication Header + SPK-ID)) |
| 0xAC0 | Boot Header Signature = RSA-4096 ( SSK, Padding \|\| SHA-384 (Boot Header)) |
| 0xCC0 | Partition Signature = RSA-4096 ( SSK, Padding \|\| SHA-384 (Partition \|\| Padding \|\| Authentication Header \|\| SPK ID \|\| UDF \|\| PPK \|\| SPK \|\| SPK Signature \|\| BH Signature)) |

> ✎ **Note:** FSBL Signature is calculated as follows:
> `FSBL Signature = RSA-4096 ( SSK, Padding || SHA-384 (PMUFW || FSBL || Padding || Authentication Header || SPK ID || UDF || PPK || SPK || SPK Signature|| BH Signature))`

#### Zynq UltraScale+ MPSoC Authentication Certification Header

The following table describes the Authentication Header bit fields for the AMD Zynq™ UltraScale+™ MPSoC device.

**Table: Authentication Header Bit Fields**

| Bit Field | Description | Notes |
| :--- | :--- | :--- |
| 31:20 | Reserved | 0 |
| 19:18 | SPK/User eFuse Select | • 01: SPK eFuse<br>• 10: User eFuse |
| 17:16 | PPK Key Select | • 0: PPK0<br>• 1: PPK1 |
| 15:14 | Authentication Certificate Format | 00: PKCS #1 v1.5 |
| 13:12 | Authentication Certificate Version | 00: Current AC |
| 11 | PPK Key Type | 0: Hash Key |
| 10:9 | PPK Key Source | 0: eFUSE |
| 8 | SPK Enable | 1: SPK Enable |
| 7:4 | Public Strength | • 0: Reserved<br>• 1: 4096<br>• 2:3: Reserved |
| 3:2 | Hash Algorithm | • 1: SHA3/384<br>• 2:3 Reserved |
| 1:0 | Public Algorithm | • 0: Reserved<br>• 1: RSA<br>• 2: Reserved<br>• 3: Reserved |

### Zynq UltraScale+ MPSoC Secure Header

When you choose to encrypt a partition, Bootgen appends the secure header to that partition. The secure header, contains the key/iv used to encrypt the actual partition. This header in-turn is encrypted using the device key and iv. The Zynq UltraScale+ MPSoC secure header is shown in the following table.

**Figure: Zynq UltraScale+ MPSoC Secure Header**

*(Diagram showing AES and AES with Key rolling structures for Partition#0 (FSBL), Partition#1, and Partition#2, detailing Secure Header, Block #0, Block #1, Block #2, etc., with Key and IV assignments)*

### Zynq UltraScale+ MPSoC Boot Image Block Diagram

The following is a diagram of the components that can be included in an AMD Zynq™ UltraScale+™ MPSoC boot image.

**Figure: Zynq UltraScale+ MPSoC Device Boot Image Block Diagram**

*(Diagram showing Boot Header, Image Header Table, Image Headers, Partition Headers, Header AC, BootLoader (FSBL and PMUFW), BootLoader AC, Partition 1, Partition 1 AC, Partition (n), Partition (n) AC)*

---

## Versal Adaptive SoC Boot Image Format

The following is a diagram of the components that can be included in an Versal adaptive SoC boot image called programmable device image (PDI).

### Platform Management Controller

The platform management controller (PMC) in Versal adaptive SoC is responsible for platform management of the Versal adaptive SoC, including boot and configuration. This chapter is focused on the boot image format processed by the two PMC MicroBlaze processors, the ROM code unit (RCU), and the platform processing unit (PPU):

**RCU**
The ROM code unit contains a triple-redundant MicroBlaze processor and read-only memory (ROM) which contains the executable BootROM. The BootROM executable is metal-masked and unchangeable. The MicroBlaze processor in the RCU is responsible for validating and running the BootROM executable. The RCU is also responsible for post-boot security monitoring and physical unclonable function (PUF) management.

**PPU**
The platform processing unit contains a triple-redundant MicroBlaze processor and 384 KB of dedicated PPU RAM. The MicroBlaze in the PPU is responsible for running the platform loader and manager (PLM).

In Versal adaptive SoC, the adaptable engine (PL) consists of rCDO and rNPI files. The rCDO file mainly contains of CFrame data along with PL and NoC power domain initialization commands. The rNPI file contains configuration data related to the NPI blocks. NPI blocks include NoC elements: NMU, NSU, NPS, NCRB; DDR memory, XPHY, XPIO, GTY, and MMCMs.

> ✎ **Note:** Versal adaptive SoC includes SSI technology devices. For more information, see SSI Technology Support.

**Figure: Versal Adaptive SoC Boot Image Block Diagram**

*(Diagram showing Boot Header, PLM Authentication Certificate, PLM, PMC CDO, Meta Headers, Image Headers, Partition Headers, and Partitions)*

**Figure: Versal Adaptive SoC Boot Image Block Diagram Part II**

*(Diagram showing Bootloader AC, Meta Header AC, Partition 1 AC, Partition k AC, and their respective components like Authentication Header, Revoke ID, User Data, PPK, SPK, Signatures)*

### Versal Adaptive SoC Boot Header

Boot header is used by PMC BootROM. Based on the attributes set in the boot header, PMC BootROM validates the Platform Loader and Manager (PLM) and loads it to the PPU RAM. The first 16 bytes are intended for SelectMAP Bus detection. PMC BootROM and PLM ignore this data so Bootgen does not include this data in any of its operations like checksum/SHA/RSA/Encryption and so on. The following code snippet is an example of SelectMAP Bus width detection pattern bits. Bootgen places the following data in the first 16 bytes as per the width selected.

The individual image header width and the corresponding bits are shown in the following list:

*   **X8**
    [LSB] 00 00 00 DD 11 22 33 44 55 66 77 88 99 AA BB CC [MSB]
*   **X16**
    [LSB] 00 00 DD 00 22 11 44 33 66 55 88 77 AA 99 CC BB [MSB]
*   **X32**
    [LSB] DD 00 00 00 44 33 22 11 88 77 66 55 CC BB AA 99 [MSB]

> ✎ **Note:** The default SelectMAP width is X32.

The following table shows the boot header format for a Versal adaptive SoC.

**Table: Versal Adaptive SoC Boot Header Format**

| Offset (Hex) | Size (Bytes) | Description | Details |
| :--- | :--- | :--- | :--- |
| `0x00` | 16 | SelectMAP bus width | Used to determine if the SelectMAP bus width is x8, x16, or x32 |
| `0x10` | 4 | QSPI bus width | QSPI bus width description. This is required to identify the QSPI flash in single/dual stacked or dual parallel mode. `0xAA995566` in the little endian format. |
| `0x14` | 4 | Image identification | Boot image identification string. Contains 4 bytes X, N, L, X in byte order, which is `0x584c4e58` in the little endian format. |
| `0x18` | 4 | Encryption key source | This field is used to identify the AES key source:<br>• `0x00000000` - Unencrypted<br>• `0xA5C3C5A3` - eFUSE red key<br>• `0xA5C3C5A5` - eFUSE black key<br>• `0x3A5C3C5A` - BBRAM red key<br>• `0x3A5C3C59` - BBRAM black key<br>• `0xA35C7C53` - Boot Header black key |
| `0x1C` | 4 | PLM source offset | PLM source start address in PDI |
| `0x20` | 4 | PMC data load address | PMC CDO address to load |
| `0x24` | 4 | PMC data length | PMC CDO length |
| `0x28` | 4 | Total PMC data length | PMC CDO length including authentication and encryption overhead |
| `0x2C` | 4 | PLM length | PLM original image size |
| `0x30` | 4 | Total PLM length | PLM image size including the authentication and encryption overhead |
| `0x34` | 4 | Boot header attributes | Versal Adaptive SoC Boot Header Attributes |
| `0x38` | 32 | Black key | 256-bit key, only valid when encryption status is set to black key in boot header |
| `0x58` | 12 | Black IV | Initialization vector used when decrypting the black key |
| `0x64` | 12 | Secure header IV | Secure header initialization vector |
| `0x70` | 4 | PUF shutter value | Length of the time the PUF samples before it closes the shutter<br>✎ **Note:** This shutter value must match the shutter value that was used during PUF registration. |
| `0x74` | 12 | Secure Header IV for PMC Data | The IV used to decrypt secure header of PMC data. |
| `0x80` | 68 | Reserved | Populate with zeroes. |
| `0xC4` | 4 | Meta Header Offset | Offset to the start of meta header. |
| `0xC8-0x124` | 96 | Reserved | |
| `0x128` | 2048 | Register init | Stores register write pairs for system register initialization |
| `0x928` | 1544 | PUF helper data | PUF helper data |
| `0xF30` | 4 | Checksum | Inverse of header checksum as per standard algorithm. |
| `0xF34` | 76 | SHA3 padding | SHA3 standard padding |

#### Versal Adaptive SoC Boot Header Attributes

The image attributes are described in the following table.

**Table: Versal Adaptive SoC Boot Header Attributes**

| Field Name | Bit Offset | Width | Default Value | Description |
| :--- | :--- | :--- | :--- | :--- |
| Reserved | [31:18] | 14 | 0x0 | Reserved for future use, Must be 0 |
| PUF Mode | [17:16] | 2 | 0x0 | 0x3 - PUF 4K mode. |
| Boot Header Authentication | [15:14] | 2 | 0x0 | 0x3 - Authentication of the boot image is done, excluding verification of PPK hash and SPK ID.<br>All others - Authentication is decided based on eFUSE RSA/ECDSA bits. |
| Reserved | [13:12] | 2 | 0x0 | Reserved for future use, Must be 0 |
| DPA countermeasure | [11:10] | 2 | 0x0 | 0x3 - Enabled<br>All others disabled. (eFUSE over rides this) |
| Checksum selection | [9:8] | 2 | 0x0 | 0x0, 0x1, 0x2 - Reserved<br>0x3 - SHA3 is used as hash function to do Checksum. |
| PUF HD | [7:6] | 2 | 0x0 | 0x3 - PUF HD is part of boot header<br>All other - PUF HD is in eFUSE. |
| Reserved | [5:0] | 6 | 0x0 | Reserved |

### Versal Adaptive SoC Image Header Table

The following table contains generic information related to the PDI image.

**Table: Versal Adaptive SoC Image Header Table**

| Offset | Name | Description |
| :--- | :--- | :--- |
| `0x0` | Version | `0x00040000`(v4.0):<br>1. Added AAD support for IHT.<br>2. Hash is included into the 32k secure chunk.<br><br>`0x00030000`(v3.0): updated secure chunk size to 32 KB from 64 KB<br>`0x00020000`(v2.00): IHT, PHT sizes doubled |
| `0x4` | Total Number of Images | Total number of images in the PDI |
| `0x8` | Image Header Offset | Word address to start of first image header |
| `0xC` | Total Number of Partitions | Total number of partitions in the PDI |
| `0x10` | Partition Header Offset | Word offset to the start of partitions headers |
| `0x14` | Secondary boot device address | Indicates the address where secondary image is present.<br>This is only valid if secondary boot device is present in attributes |
| `0x1C` | Image Header Table Attributes | Refer to Image Header Table Attributes |
| `0x20` | PDI ID | Used to identify a PDI |
| `0x24` | Parent ID | ID of initial boot PDI. For boot PDI, it is same as the PDI ID |
| `0x28` | Identification string | Full PDI if present with boot header – “FPDI”<br>Partial/Sub-system PDI – “PPDI” |
| `0x2C` | Headers size | 0-7: Image header table size in words<br>8-15: Image header size in words<br>16-23: Partition header size in words<br>24-31: Reserved |
| `0x30` | Total meta header length (Word) | Including authentication and encryption overhead (excluding IHT and including AC) |
| `0x34 -0x3C` | IV for encryption of meta header | IV for decrypting SH of header table |
| `0x40` | Encryption status | Encryption key source, only key source used for PLM is valid for meta header.<br>• `0x00000000` - Unencrypted<br>• `0xA5C3C5A3` - eFuse red key<br>• `0xA5C3C5A5` - eFUSE black key<br>• `0x3A5C3C5A` - BBRAM red key<br>• `0x3A5C3C59` - BBRAM black key<br>• `0xA35C7C53` - Boot Header black key |
| `0x48` | Meta Header AC Offset (Word) | Word Offset to Meta Header Authentication Certificate |
| `0x4c` | Meta Header Black/IV | IV that is used to encrypt the Black key used to encrypt the Meta Header. |
| `0x58` | Optional Data Length (Word) | Size of Optional Data available in Bootloader |
| `0x5C - 0x78` | Reserved | `0x0` |
| `0x7C` | Checksum | Inverse of sum of all the previous words as per standard algorithm in the image header table |

#### Image Header Table Attributes

The image header tables are described in the following table.

**Table: Versal Adaptive SoC Image Header Table Attributes**

| Bit Field | Name | Description |
| :--- | :--- | :--- |
| 31:14 | Reserved | 0 |
| 14 | PUF Helper Data Location | Location of the PUF Helper Data efuse/BH |
| 12 | dpacm enable | DPA Counter Measure enable or not |
| 11:6 | Secondary boot device | Indicates the device on which rest of the data is present in.<br>0 - Same boot device (default)<br>1 - QSPI32<br>2 - QSPI24<br>3 - Reserved<br>4 - SD0<br>5 - SD1<br>6 - SDLS<br>7 - EMMC/ MMC<br>8 - USB<br>9 - Reserved<br>10 - PCIe<br>11 - Reserved<br>12 - OSPI<br>13 - SMAP<br>14 - SBI<br>15 - SD0RAW<br>16 - SD1RAW<br>17 - SDLSRAW<br>18 - MMCRAW<br>19 - MMC0<br>20 - MMC0RAW<br>21- imagestore<br>All others are reserved<br>✎ **Note:** These options are supported for various devices in Bootgen. For the exact list of secondary boot devices supported by any device, refer to its corresponding Systems Software Developers Guide (SSDG). |
| 5:0 | Reserved | |

#### Optional Data

Optional data is binary data placed in the PDI after the image header table (IHT). You can include your own optional data like data, version, and so on. For more information, refer to optionaldata. This data is authenticated as part of IHT Signature and IHT is added as additional authenticated data (AAD) for the first secure header (SH) of the image header (IH) during encryption along with IHT. This implies that the optional data content remains as plain text.

As part of Authentication Optimization, for partitions that use the same authentication keys, the partition hashes are placed as part of Optional Data. For more information, see Authentication Optimization.

Bootgen accepts user and multiple optional data. For information on adding optional data, see optionaldata.

**Table: Versal Adaptive SoC Optional Data**

| Offset | Name | Description |
| :--- | :--- | :--- |
| `0x0` | ID | The Data IDs from `0x0` to `0x20` for Optional Data are reserved for internal use. User Optional Data ID can be anything > `0x20`.<br>0 - None, can be used for padding<br>1 - PLM Build Time Configuration Metadata as defined in the PLM Configuration document<br>2 - Data structure version information used in In-Place PLM Update Compatibility Check<br>3 - Hash Table for authentication optimization<br>4 - Indicates presence of PSMFW |
| `0x2` | Size | Total optional data size in words |
| `0x4` | Data | Actual data including padding |
| Last | Checksum | Sum of all the previous words in this data structure |

### Versal Adaptive SoC Image Header

The image header is an array of structures containing information related to each image, such as an ELF file, CFrame, NPI, CDOs, data files, and so forth. Each image can have multiple partitions, for example, an ELF can have multiple loadable sections, each of which form a partition in the boot image. An image header points to the partitions (partition headers) that are associated with this image. Multiple partition files can be grouped within an image using the BIF keyword "image"; this is useful for combining all the partitions related to a common subsystem or function in a group. Bootgen creates the required partitions for each file and creates a common image header for that image. The following table contains the information of number of partitions related to an image.

**Table: Versal Adaptive SoC Image Header**

| Offset | Name | Description |
| :--- | :--- | :--- |
| `0x0` | First Partition Header (Word) | Word offset to first partition header |
| `0x4` | Number of Partitions | Number of partitions present for this image |
| `0x8` | Revoke ID | Revoke ID for Meta Header |
| `0xC` | Image Attributes | See Image Attributes table |
| `0x10-0x1C` | Image Name | ASCII name of the image. Max of 16 characters. Fill with Zeros when padding is required. |
| `0x20` | Image/Node ID | Defines the resource node the image is initializing |
| `0x24` | Unique ID | Defines the affinity/compatibility identifier when required for a given device resource |
| `0x28` | Parent Unique ID | Defines the required parent resource UID for the configuration content of the image, if required |
| `0x2c` | Function ID | Identifier used to capture the unique function of the image configuration data |
| `0x30` | DDR Low Address for Image Copy | The DDR lower 32-bit address where the image must be copied when memcpy is enabled in BIF |
| `0x34` | DDR High Address for Image Copy | The DDR higher 32-bit address where image must be copied when memcpy is enabled in BIF |
| `0x38` | Reserved | |
| `0x3C` | Checksum | Inverse of sum of all the previous words as per standard algorithm. |

The following table shows the Image Header Attributes.

**Table: Versal Adaptive SoC Image Header Attributes**

| Bit Field | Name | Description |
| :--- | :--- | :--- |
| 31:9 | Reserved | 0 |
| 8 | Delay Hand off | 0 – Handoff the image now (default)<br>1 – Handoff the image later |
| 7 | Delay load | 0 – Load the image now (default)<br>1 – Load the image later |
| 6 | Copy to memory | 0 – No copy to memory (Default)<br>1 – Image to be copied to memory |
| 5:3 | Image Owner | 0 - PLM (default)<br>1 - Non-PLM<br>2-7 – Reserved |
| 2:0 | Reserved | 0 |

### Versal Adaptive SoC Partition Header

The partition header contains details of the partition and is described in the table below.

**Table: Versal Adaptive SoC Partition Header Table**

| Offset | Name | Description |
| :--- | :--- | :--- |
| `0x0` | Partition Data Word Length | Encrypted partition data length |
| `0x4` | Extracted Data Word Length | Unencrypted data length |
| `0x8` | Total Partition Word Length (Includes Authentication Certificate) | The total encrypted + padding + expansion + authentication length |
| `0xC` | Next Partition header offset (Word) | Offset of next partition header |
| `0x10` | Destination Execution Address (Lower Half) | The lower 32-bit of the executable address of this partition after loading. |
| `0x14` | Destination Execution Address (Higher Half) | The higher 32-bit of the executable address of this partition after loading. |
| `0x18` | Destination Load Address (Lower Half) | The lower 32-bit of the RAM address into which this partition is to be loaded. For elf files, Bootgen automatically reads from elf format. For RAW data, you have to specify where to load it. For CFI and configuration data it must be `0xFFFF_FFFF` |
| `0x1C` | Destination Load Address (Higher Half) | The higher 32-bit of the RAM address into which this partition is to be loaded. For elf files, Bootgen automatically reads from elf format. For RAW data, you have to specify where to load it. For CFI and configuration data it must be `0xFFFF_FFFF` |
| `0x20` | Data Word Offset in Image | The position of the partition data relative to the start of the boot image. |
| `0x24` | Attribute Bits | See Partition Attributes Table |
| `0x28` | Section Count | If image type is elf, it specifies number of additional partitions associated with the elf. |
| `0x2C` | Checksum Word Offset | The location of the checksum word in the boot image. |
| `0x30` | Partition ID | Partition ID |
| `0x34` | Authentication Certification Word Offset | The location of the Authentication Certification in the boot image. |
| `0x38 – 0x40` | IV | IV for the secure header of the partition. |
| `0x44` | Encryption Key select | Encryption status:<br>• `0x00000000` – Unencrypted<br>• `0xA5C3C5A3` - eFUSE Red Key<br>• `0xA5C3C5A5` - eFuse Black Key<br>• `0x3A5C3C5A` - BBRAM Red Key<br>• `0x3A5C3C59` - BBRAM Black Key<br>• `0xA35C7C53` - Boot Header Black Key<br>• `0xC5C3A5A3` - User Key 0<br>• `0xC3A5C5B3` - User Key 1<br>• `0xC5C3A5C3` - User Key 2<br>• `0xC3A5C5D3` - User Key 3<br>• `0xC5C3A5E3` - User Key 4<br>• `0xC3A5C5F3` - User Key 5<br>• `0xC5C3A563` - User Key 6<br>• `0xC3A5C573` - User Key 7<br>• `0x5C3CA5A3` - eFuse User Key 0<br>• `0x5C3CA5A5` - eFuse User Black Key 0<br>• `0xC3A5C5A3` - eFuse User Key 1<br>• `0xC3A5C5A5` - eFuse User Black Key 1 |
| `0x48` | Black IV | IV used for encrypting the key source of that partition. |
| `0x54` | Revoke ID | Partition revoke ID |
| `0x58-0x78` | Reserved | 0 |
| `0x7C` | Header Checksum | Inverse of sum of the previous words is as per standard algorithm in the Partition Header. |

The following table lists the partition header table attributes.

**Table: Versal Adaptive SoC Partition Header Table Attributes**

| Bit Field | Name | Description |
| :--- | :--- | :--- |
| 31:29 | Reserved | `0x0` |
| 28:27 | DPA CM Enable | 0 – Disabled<br>1 – Enabled |
| 26:24 | Partition Type | 0 – Reserved<br>1 - elf<br>2 - Configuration Data Object<br>3 - Cframe Data (PL data)<br>4 – Raw Data<br>5 – Raw elf<br>6 – CFI GSR CSC unmask frames<br>7 – CFI GSR CSC mask frames |
| 23 | HiVec | VInitHi setting for RPU/APU(32-bit) processor<br>0 – LoVec<br>1 – HiVec |
| 22:19 | Reserved | 0 |
| 18 | Endianness | 0 – Little Endian (Default)<br>1 – Big Endian |
| 17:16 | Partition Owner | 0 - PLM (Default)<br>1 - Non-PLM<br>2,3 – Reserved |
| 15:14 | PUF HD location | 0 - eFuse<br>1 - Boot header |
| 13:12 | Checksum Type | 000b - No Checksum(Default)<br>011b – SHA3 |
| 11:8 | Destination CPU | 0 – None (Default for non-elf files)<br>1 - A72-0<br>2 - A72-1<br>3 - Reserved<br>4 - Reserved<br>5 - R5-0<br>6 - R5-1<br>7- R5-L<br>8 – PSM<br>9 - AIE<br>10-15 – Reserved |
| 4:7 | Reserved | `0x0` |
| 3 | A72 CPU execution state | 0 - Aarch64 (default)<br>1 - Aarch32 |
| 2:1 | Exception level (EL) that the A72 core must be configured for | 00b – EL0<br>01b – EL1<br>10b – EL2<br>11b – EL3 (Default) |
| 0 | TZ secure partition | 0 – Non-Secure (Default)<br>1 – Secure<br>This bit indicates if the core that the PLM needs to configure (on which this partition needs to execute) must be configured as TrustZone secure or not. By default, this must be 0. |

### Versal Adaptive SoC Authentication Certificates

The Authentication Certificate is a structure that contains all the information related to the authentication of a partition. This structure has the public keys and the signatures that BootROM/PLM needs to verify. There is an Authentication Header in each Authentication Certificate, which gives information like the key sizes, algorithm used for signing, and so forth. Unlike the other devices, the Authentication Certificate is prepended or attached to the beginning of the actual partition, for which authentication is enabled. If you want Bootgen to perform authentication on the meta headers, specify it explicitly under the ‘metaheader’ bif attribute. See BIF Attribute Reference for information on usage.

Versal adaptive SoC uses RSA-4096 authentication and ECDSA algorithms for authentication. The following table provides the format of the Authentication Certificate for the Versal adaptive SoC.

**Table: Versal Adaptive SoC Authentication Certificate – ECDSA p384**

| Authentication Certificate Bits | Description |
| :--- | :--- |
| `0x00` | Authentication Header. See Versal Adaptive SoC Authentication Certification Header |
| `0x04` | Revoke ID |
| `0x08` | UDF (56 bytes) |
| `0x40` | PPK<br>x (48 bytes)<br>y (48 bytes)<br>Pad `0x00` (932 bytes) |
| `0x444` | PPK SHA3 Pad (12 bytes) |
| `0x450` | SPK<br>x (48 bytes)<br>y (48 bytes)<br>Pad `0x00` (932 bytes) |
| `0x854` | SPK SHA3 Pad (4 bytes) |
| `0x858` | Alignment (8 bytes) |
| `0x860` | SPK Signature(r+s+pad)(48+48+416) |
| `0xA60` | BH/IHT Signature(r+s+pad)(48+48+416) |
| `0xC60` | Partition Signature(r+s+pad)(48+48+416) |

**Table: Versal Adaptive SoC Authentication Certificate – ECDSA p521**

| Authentication Certificate Bits | Description |
| :--- | :--- |
| `0x00` | Authentication Header. See Versal Adaptive SoC Authentication Certification Header |
| `0x04` | Revoke ID |
| `0x08` | UDF (56 bytes) |
| `0x40` | PPK<br>PPK x (66 bytes)<br>y (66 bytes)<br>Pad `0x00` (896 bytes) |
| `0x444` | PPK SHA3 Pad (12 bytes) |
| `0x450` | SPK<br>SPK x (66 bytes)<br>y (66 bytes)<br>Pad `0x00` (896 bytes) |
| `0x854` | SPK SHA3 Pad (4 bytes) |
| `0x858` | Alignment (8 bytes) |
| `0x860` | SPK Signature(r+s+pad)(66+66+380) |
| `0xA60` | BH/IHT Signature(r+s+pad)(66+66+380) |
| `0xC60` | Partition Signature(r+s+pad)(66+66+380) |

**Table: Versal Adaptive SoC Authentication Certificate – RSA**

| Authentication Certificate Bits | Description |
| :--- | :--- |
| `0x00` | Authentication Header. See Versal Adaptive SoC Authentication Certification Header |
| `0x04` | Revoke ID |
| `0x08` | UDF (56 bytes) |
| `0x40` | PPK<br>Mod (512 bytes)<br>Mod Ext (512 bytes)<br>Exponent (4 bytes) |
| `0x444` | PPK SHA3 Pad (12 bytes) |
| `0x450` | SPK<br>Mod (512 bytes)<br>Mod Ext (512 bytes)<br>Exponent (4 bytes) |
| `0x854` | SPK SHA3 Pad (4 bytes) |
| `0x858` | Alignment (8 bytes) |
| `0x860` | SPK Signature |
| `0xA60` | BH/IHT Signature |
| `0xC60` | Partition Signature |

#### Versal Adaptive SoC Authentication Certification Header

The following table describes the Authentication Header bit fields for the Versal adaptive SoC.

**Table: Authentication Header Bit Fields**

| Bit Fields | Description | Notes |
| :--- | :--- | :--- |
| 31:16 | Reserved | 0 |
| 15-14 | Authentication Certificate Format | 00 -RSAPSS |
| 13-12 | Authentication Certificate Version | 00: Current AC |
| 11 | PPK Key Type | 0: Hash Key |
| 10-9 | PPK Key Source | 0: eFUSE |
| 8 | SPK Enable | 1: SPK Enable |
| 7-4 | Public Strength | 0 - ECDSA p384<br>1 - RSA 4096<br>2 - ECDSA p521 |
| 3-2 | Hash Algorithm | 1-SHA3 |
| 1-0 | Public Algorithm | 1-RSA<br>2-ECDSA |

1.  For the Bootloader partition:
    a. The offset `0xA60` of the AC holds the Boot Header Signature.
    b. The offset `0xC60` of the AC holds the signature of PLM and PMCDATA.
2.  For the Header tables
    a. The offset `0xA60` of the AC holds the IHT Signature.
    b. The offset `0xC60` of the AC holds the signature of all the headers except IHT.
3.  For any other partition:
    a. The offset `0xA60` of the AC is zeroized.
    b. The offset `0xC60` of the AC holds the signature of that partition.

---

## Spartan UltraScale+ Boot Image Format

**Figure: Spartan UltraScale+ Boot Image Block Diagram (.PDI)**

*(Diagram showing Boot Header, Authentication Certificate (PLM + PL Data), Hash Block Signature, Hash Block, PLM + PL Data, and their respective components like PPK, SPK Header, SPK, SPK Signature, BH Hash, PLM, PL Data, Padding)*

The following figure illustrates Spartan UltraScale+ boot Image for PUF helper data (.PDI):

**Figure: Spartan UltraScale+ Boot Image Block Diagram for PUF Helper Data**

*(Diagram showing Boot Header, PUF Data\*, Authentication Certificate (PLM\* + PL Data\*), Hash Block Signature, Hash Block, PLM\* + PL Data\*)*
\*Indicates Optional

> ✎ **Note:**
> *   Exception: PUF and PL data only is an invalid combination.
> *   If PUF data is only present, then only BH and PUF data present in the PDI.
> *   Following is the sample command to generate separate PDI with only PUF data:
>     `bootgen -arch spartanup -image <BIF> -w -o <PDI> -dump puf_pdi`

### Spartan UltraScale+ Boot Header

The boot header is used by BootROM in Spartan UltraScale+ FPGAs. Based on the attributes set in the boot header, it validates the platform loader and manager (PLM) and loads it to the PMC RAM. The initial 16 bytes are required for SelectMAP bus width detection. BootROM/PLM ignores this data, so Bootgen must not include this data in any of the operations, for example, checksum, SHA, and encryption. The boot header is always in the plain text.

**Table: Spartan UltraScale+ Boot Header**

| Offset | Size (Bytes) | Name | Description |
| :--- | :--- | :--- | :--- |
| `0x0` | 16 | SelectMAP Bus Width Words | Used to determine if the SelectMAP bus width is x8, x16, or x32. |
| `0x10` | 4 | Width Detection | QSPI bus width description. This is required to identify the QSPI flash in single or dual stacked mode. `0xAA995566` in little endian format. |
| `0x14` | 4 | Image Identification | Boot image identification string. Contains four bytes (X, N, L, and X) in byte order, which is `0x584c4e58` in the little endian format. |
| `0x18` | 4 | Encryption Key Source | AES key source and type:<br>• `0x00000000` Un-encrypted<br>• `0xA5C3C5A3` eFUSE Key<br>• `0xA5C3C5A5` eFUSE PUF KEK<br>• `0xA5C3C5A7` eFUSE Family KEK<br>• `0xA35C7C53` BH PUF KEK<br>• `0xA35C7CA5` BH Family KEK |
| `0x1C` | 4 | Source Offset | Source offset of PLM in PDI |
| `0x20` | 4 | PL Data Load Address | Data partition start address to load |
| `0x24` | 4 | PL Data Length | Data partition size |
| `0x28` | 4 | Total PL Data Length | Data partition size including authentication and encryption overhead |
| `0x2C` | 4 | PLM Length | PLM size |
| `0x30` | 4 | Total PLM Length | PLM image size including authentication and encryption overhead |
| `0x34` | 4 | Boot Header Attributes | Refer to the table in Spartan UltraScale+ Boot Header Attributes |
| `0x38` | 32 | Grey/Black Key | Boot header black/grey key storage |
| `0x58` | 12 | Grey/Black IV | Boot header black/grey IV storage |
| `0x64` | 12 | Secure Header IV for Hash Block | Secure header IV |
| `0x70` | 4 | Encryption Revocation ID | Encryption revocation ID |
| `0x74` | 4 | Authentication Header - 1 | Hash algorithm selection, authentication algorithm is explained in Spartan UltraScale+ Authentication Certificate. |
| `0x78` | 4 | HASH Block Size - 1 | HASH block size filled with zeros (for optional block) |
| `0x7C` | 4 | Total PPK Size - 1 | PPK Size with padding to make it aligned |
| `0x80` | 4 | Actual PPK Size - 1 | Actual PPK 1 size |
| `0x84` | 4 | Total PDI Signature Size - 1 | Signature size with padding to make it aligned |
| `0x88` | 4 | Actual PDI Signature Size - 1 | Actual PDI signature size 1 |
| `0x8C` | 4 | PUF Image Id | PUF PDI identification string |
| `0x90` | 4 | PUF Shutter Value | PUF shutter value |
| `0x94` | 4 | Ring Oscillator Config Value | Ring oscillator value, default is zero |
| `0x98` | 4 | PUF HD Length | PUF helper data size |
| `0x9C` | 60 | ROM Reserved | Reserved, fill with zeroes |
| `0xD8` | 4 | User Defined Component Revision | User defined component revision |
| `0xDC` | 96 | PLM Reserved | Reserved, used by PLM |
| `0x13C` | 512 | Reg Init | Register initialization |
| `0x33C` | 4 | Check Sum | Checksum is calculated at `0x338` |

#### Spartan UltraScale+ Boot Header Attributes

Image attributes field comprises packed information of the boot image attributes and details are tabulated below.

**Table: Spartan UltraScale+ Boot Header Attributes**

| Field Name | Bit Offset | Width | Default Value | Description |
| :--- | :--- | :--- | :--- | :--- |
| Reserved | [31:20] | 12 | `0x0` | Reserved (must be 0) |
| Signed Image | [19:18] | 2 | `0x0` | • 0x3: Signed image<br>• All others: Unsigned image |
| PUF Mode | [17:16] | 2 | `0x0` | • 0x3: PUF 4K mode<br>• All others: Reserved |
| BH Authentication | [15:14] | 2 | `0x0` | • 0x3: Authentication of the boot image is done, excluding verification of PPK hash and SPK ID<br>• All others: Authentication is decided based on authentication enablement in eFuses |
| Reserved | [13:12] | 2 | `0x0` | Reserved (must be 0) |
| DPA CM | [11:10] | 2 | `0x0` | • 0x3: Enabled<br>• All others disabled (eFuse overrides this) |
| BI Integrity Selection | [9:8] | 2 | `0x0` | • 0x0, 0x1, and 0x2: Reserved<br>• 0x3: SHA3 is used as hash function to do BI integrity check |
| PUF HD | [7:6] | 2 | `0x0` | • 0x3: PUF HD is part of boot header<br>• All others: PUF HD is in PUF HD PDI |
| Reserved | [5:4]<br>[3:0] | 2<br>4 | `0x0` | Reserved |

### Spartan UltraScale+ Authentication Certificate

The authentication certificate is a structure that contains all the information related to the authentication of a partition. This structure has the public keys and the signatures that BootROM/PLM needs to verify. There is an authentication header in each authentication certificate, which gives information like the key sizes and algorithm used for signing. The authentication certificate is attached to the beginning of the actual partition PLM/PL data, for which authentication is enabled. Spartan UltraScale+ SU10P, SU25P, and SU35P use the HSS-SHAKE256 authentication, where as Spartan UltraScale+ SU45P, SU60P, SU65P, SU100P, SU150P, and SU200P use the HSS-SHAKE256, LMS-SHAKE256, and ECDSA P-384 authentications. The following tables provide the format of the authentication certificates.

**Table: Spartan UltraScale+ Authentication Certificate – ECDSA P-384**

| Authentication Certificate Bits | Description |
| :--- | :--- |
| `0x00` | PPK<br>x (48 bytes) – (coordinate )<br>y (48 bytes) – (coordinate) |
| `0x60` | Total SPK size |
| `0x64` | Actual SPK size |
| `0x68` | Total SPK signature size |
| `0x6C` | Actual SPK signature size |
| `0x70` | SPK Revocation ID |
| `0x74` | SPK header alignment |
| `0x80` | SPK<br>x (48 bytes) – (coordinate )<br>y (48 bytes) – (coordinate) |
| `0xE0` | SPK signature |

> ✎ **Note:** Authentication certificate ECDSA P-384 is not applicable for SU10P, SU25P, and SU35P.

**Table: Spartan UltraScale+ Authentication Certificate – LMS and HSS**

| Authentication Certificate Bits | Description |
| :--- | :--- |
| `0x00` | PPK |
| `0x3c` | PPK alignment |
| `0x40` | Total SPK size |
| `0x44` | Actual SPK size |
| `0x48` | Total SPK sign size |
| `0x4C` | Actual SPK sign size |
| `0x50` | SPK Revocation ID |
| `0x54` | SPK header alignment |
| `0x60` | SPK |
| `0x9C` | SPK align |
| `0xA0` | SPK sign |

> ✎ **Note:** Refer to LMS Key Expiration Error for more details.

#### Spartan UltraScale+ Authentication Header

The following table shows header defines details of boot authentication scheme for Spartan UltraScale+ PDI.

**Table: Spartan UltraScale+ Authentication Header Table (SU10P, SU25P, and SU35P)**

| Bits | Field | Spartan UltraScale+ Option | Size (Bits) |
| :--- | :--- | :--- | :--- |
| [31:16] | Reserved | Reserved | 16 |
| [15:14] | Authentication Certificate Format | Reserved | 2 |
| [13:12] | Authentication Certificate Version | 00: Current AC | 2 |
| [11] | PPK Key Type | 0: Hash Key | 1 |
| [10:9] | PPK Key Src | 0: eFUSE | 2 |
| [8] | SPK Enable | 1: SPK Enabled | 1 |
| [7:4] | Public Strength | Reserved | 4 |
| [3:2] | Hash Algo | • 0-2: Reserved<br>• 3: SHAKE/256 | 2 |
| [1:0] | Public Algo | • 3: LMS with HSS<br>• All others: Reserved | 2 |

**Table: Spartan UltraScale+ Authentication Header Table (SU45P/SU60P/SU65P/SU100P/SU150P/SU200P)**

| Bits | Field | Spartan UltraScale+ Option | Size (Bits) |
| :--- | :--- | :--- | :--- |
| [31:16] | Reserved | Reserved | 16 |
| [15:14] | Authentication Certificate Format | Reserved | 2 |
| [13:12] | Authentication Certificate Version | 00: Current AC | 2 |
| [11] | PPK Key Type | 0: Hash Key | 1 |
| [10:9] | PPK Key Src | 0: eFUSE | 2 |
| [8] | SPK Enable | 1: SPK Enabled | 1 |
| [7:4] | Hash Algo | • 0: Reserved (for Sha2-384, which is not supported in SU45P/SU60P/SU65P/SU100P/SU150P/SU200P)<br>• 1: SHA3/384<br>• 2 to 16: Reserved | 4 |
| [3:0] | Public Algo | • 0: Non-Secure<br>• 1: Reserved<br>• 2: ECDSAp384<br>• 3: Reserved<br>• 4: LMS with HSS<br>• 5: LMS<br>• 6 to 16: Reserved | 4 |

---

## Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Boot Image Format

The following figure displays the components included in a Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Bootgen image called programmable device image (PDI).

**Figure: Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Boot Image Format**

*(Diagram showing Boot Header, Authentication Certificate (Hash Block0), Hash Block0, Hash Block0 Signature, PLM + PMC Partition, MetaHeader, Image Header Table, Optional Data, Image Header 0, Image Header 1, Image Header N, Partition Header 0, Partition Header 1, Partition Header N, Authentication Certificate(Hash Block1), Hash Block 1, Hash Block 1 signature, Partition 1, Partition N, and their respective components like PPK, SPK Header, SPK, SPK Signature, BH Hash, PLM Hash Hash, PMC Hash, Hash Block 1 Hash, Padding, MetaHeader Hash(IHT+ Optional data + IHS + PHS), Partition1 Hash, Partition2 Hash, Partition3 Hash, PartitionN Hash)*

### Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Boot Header

The boot header is used by BootROM in Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 devices. Based on the attributes set in the boot header, it validates the platform loader and manager (PLM) and loads it to the PMC RAM. The initial 16 bytes are reserved for SelectMAP bus width detection. BootROM/PLM ignores this data, so Bootgen does not include this data in any of the operations for example checksum, SHA, and encryption. The boot header is always in plain text.
The following are the values in binaries used for detecting SMAP width while in table it is pattern used for that purpose.

*   X8: [LSB] 00 00 00 DD 11 22 33 44 55 66 77 88 99 AA BB CC [MSB]
*   X16: [LSB] 00 00 DD 00 22 11 44 33 66 55 88 77 AA 99 CC BB [MSB]
*   X32: [LSB] DD 00 00 00 44 33 22 11 88 77 66 55 CC BB AA 99 [MSB]

> ✎ **Note:** The default SelectMAP width is X32.

**Table: Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Boot Header Table**

| Start Offset (Dec) | Start Offset (Hex) | Size (B) | Size (words) | Field | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 0 | `0x0` | 16 | 4 | SMAP Bus Width Words | Used to determine the SMAP bus width<br>X8: [LSB] 00 00 00 DD 11 22 33 44 55 66 77 88 99 AA BB CC [MSB]<br>X16: [LSB] 00 00 DD 00 22 11 44 33 66 55 88 77 AA 99 CC BB [MSB]<br>X32: [LSB] DD 00 00 00 44 33 22 11 88 77 66 55 CC BB AA 99 [MSB] |
| 16 | `0x10` | 4 | 1 | Width detection | QSPI bus width description. This is required to identify the QSPI flash in single/dual stacked or dual parallel mode. `0xAA995566` in the little-endian format. |
| 20 | `0x14` | 4 | 1 | Image identification | Boot image identification string. Contains 4 bytes X, N, L, X in byte order, which is `0x584c4e58` in the little-endian format. |
| 24 | `0x18` | 4 | 1 | Encryption status | AES Key Source and Type |
| 28 | `0x1C` | 4 | 1 | Source offset | Source offset of PMCFW in DPI |
| 32 | `0x20` | 4 | 1 | PMC CDO load address | Data partition start address to load |
| 36 | `0x24` | 4 | 1 | PMC CDO length | Data partition size |
| 40 | `0x28` | 4 | 1 | Total PMC CDO length | Data partition size including authentication and encryption overhead |
| 44 | `0x2C` | 4 | 1 | PLM length | PMCFW size |
| 48 | `0x30` | 4 | 1 | Total PLM length | PMCFW partition size including authentication and encryption overhead |
| 52 | `0x34` | 4 | 1 | Image attributes | Image Attributes |
| 56 | `0x38` | 32 | 8 | Grey/Black Key | Boot header black/Grey key storage |
| 88 | `0x58` | 12 | 3 | Grey/Black IV | Boot header black/Grey IV storage |
| 100 | `0x64` | 12 | 3 | Secure Header IV | Secure header IV |
| 112 | `0x70` | 4 | 1 | PUF Shutter Value | PUF shutter value |
| 116 | `0x74` | 4 | 1 | Ring Oscillator Config value | Ring Oscillator Value, Default is zero |
| 120 | `0x78` | 4 | 1 | Partition revocation ID | Partition revocation ID |
| 124 | `0x7C` | 516 | 129 | User Data | User Data (Default - fill with zeroes) |
| 640 | `0x280` | 4 | 1 | Authentication Header-1 | Authentication Header |
| 644 | `0x284` | 4 | 1 | HASH Block Size - 1 | HASH Block size - Filled with Zeros (for optional block) |
| 648 | `0x288` | 4 | 1 | Total PPK Size - 1 | PPK Size with padding to align |
| 652 | `0x28C` | 4 | 1 | Actual PPK Size - 1 | Actual PPK 1 Size |
| 656 | `0x290` | 4 | 1 | Total PDI Signature Size - 1 | Signature Size with padding to align |
| 660 | `0x294` | 4 | 1 | Actual PDI Signature Size - 1 | Actual PDI Signature 1 Size |
| 664 | `0x298` | 56 | 14 | ROM Reserved | Reserved. Fill with zeroes |
| 720 | `0x2D0` | 100 | 25 | PLM Reserved | Reserved for PLM |
| 820 | `0x334` | 2048 | 512 | Reg Init | Register initialization. Same as that of Alto |
| 2868 | `0xB34` | 1544 | 386 | PUF HD | PUF Helper data |
| 4412 | `0x113C` | 4 | 1 | Check Sum | checksum is calculated from `0x10` till `0x1138` First, sum all the values then negate that and extract 4 bytes out of it. |

#### Versal AI Edge Gen 2 and Versal Prime Gen 2 Boot Header Attributes

Image attributes field comprises packed information of the boot image attributes and details are available in the following table:

**Table: Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Boot Header Attributes**

| Field Name | Bit Offset | Width | Default Value | Description |
| :--- | :--- | :--- | :--- | :--- |
| Rsvd | [31:22] | 10 | `0x0` | Reserved, must be 0 |
| CDI generation | [21:20] | 2 | `0x0` | `0x3` – Generate DICE CDI<br>All others – DICE CDI generation is based on eFuse UDS programmed |
| Signed image | [19:18] | 2 | `0x0` | `0x3`- Signed image<br>All others – Unsigned image |
| Puf Mode | [17:16] | 2 | `0x0` | `0x3` - PUF 4K mode.<br>`0x0` - PUF 12K mode. |
| BH Authentication | [15:14] | 2 | `0x0` | `0x3` - Authenticates the boot image, excluding verification of PPK hash and SPK ID. All others: Authentication is decided based on eFUSE RSA/ECDSA bits. |
| Rsvd | [13:12] | 2 | `0x0` | Reserved, must be 0 |
| DPA CM | [11:10] | 2 | `0x0` | `0x3` - Enabled<br>All others disabled. (eFUSE overrides this) |
| BI Integrity selection. | [9:8] | 2 | `0x0` | `0x0`, `0x1`, `0x2` - Reserved<br>`0x3` - SHA3: used as hash function to perform BI integrity check. |
| PUF HD | [7:6] | 2 | `0x0` | `0x3` - PUF HD is part of boot header<br>All other - PUF HD is in eFUSE. |
| Rsvd | [5:4] | 2 | `0x0` | Reserved |
| Rsvd | [3:0] | 4 | `0x0` | Reserved |

### Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Image Header Table

This includes information about the number of images and partitions present. The Image Header Table consists of a header followed by a linked list of image headers. Each image header is linked to the next through a pointer, allowing Bootgen to navigate through the images efficiently.
The following table contains generic information related to the PDI image.

**Table: Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Image Header Table**

| Offset | Name | Description |
| :--- | :--- | :--- |
| `0x0` | Version | `0x00010000` (v1.00) |
| `0x4` | Total Number of Images | Maximum 32 Images |
| `0x8` | Image header offset | Address to start of first Image header |
| `0xC` | Total Number of Partition | Max 32 Partitions |
| `0x10` | Partition Header Offset | Offset to the start of partitions headers |
| `0x14` | Secondary boot device address | Indicates the address where secondary image is present.<br>This is only valid if secondary boot device is present in attributes |
| `0x18` | ID Code | Device ID code |
| `0x1C` | Image Header Table Attributes | Refer to Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Image Header Table Attributes |
| `0x20` | PDI ID | User defined field for information like revision. |
| `0x24` | Reserved | |
| `0x28` | Identification string | Full PDI if present with boot header – “FPDI”<br>Partial/Sub-system PDI – “PPDI” |
| `0x2C` | Header sizes | 0 – 7: Image header table size in words<br>8 – 15: Image header size in words<br>16 – 23: Partition header size in words<br>24 – 31: Reserved |
| `0x30` | Total meta header length | Including Authentication and encryption overhead (excluding IHT and including AC) |
| `0x34 -0x3C` | IV for encryption of headers | IV for decrypting (SecureHeader) of header table |
| `0x40` | Encryption status | Encryption key source, only key source used for PLM is valid for meta header<br>`0x00000000` - Unencrypted<br>`0xA5C3C5A3` - eFUSE Key<br>`0xA5C3C5A5` - eFUSE Black Key<br>`0xA5C3C5A7` - eFUSE Obfuscated Key<br>`0x3A5C3C5A` - BBRAM Key<br>`0x3A5C3C59` - BBRAM Black Key<br>`0x3A5C3C57` - BBRAM Obfuscated Key<br>`0xA35C7C53` - Boot Header Black Key<br>`0xA35C7CA5` - Boot Header Obfuscated Key |
| `0x44` | Extended ID Code | Extended ID Code |
| `0x48` | Hash block AC Offset of meta header | Holds authentication certificate offset of meta header Hash block |
| `0x4C – 0x54` | KEK IV of meta header | KEK IV of meta header KEK |
| `0x58` | Size of IHT Optional Data | Number of words in IHT Optional Data. This must be a multiple of 4 to align the optional data on 16 bytes. |
| `0x5C` | Authentication Header | Hash algorithm selection and authentication algorithm |
| `0x60` | Hash block length | Hash block length for Meta header |
| `0x64` | Hash block offset | Meta header hash block offset |
| `0x68` | Total PPK Size | Includes key alignment length in case, else actual PPK size |
| `0x6C` | Actual PPK Size | Actual PPK size |
| `0x70` | Total Hash block signature size | Hash block signature Size with padding to make it aligned |
| `0x74` | Actual Hash block signature size | Actual Hash block signature size |
| `0x78` | Reserved | `0x0` |
| `0x7C` | Checksum | A sum of all the previous words in the image header table |

#### Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Image Header Table Attributes

**Table: Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Image Header Table Attributes**

| Bit Field | Field Name | Description | User input | Exportable |
| :--- | :--- | :--- | :--- | :--- |
| 31:19 | Reserved | 0 | No | N/A |
| 19:18 | Reserved | 0 | No | N/A |
| 17:16 | ID Code Check | `0x00` - Verify the ID code before loading PDI (default)<br>`0x11` - ID code is not verified before loading PDI | Yes | Yes |
| 15:14 | PUF HD Location | `0x00` - PUF HD is in eFuse.<br>`0x11` - PUF HD is part of boot header | Yes | Yes |
| 13 – 12 | DPA CM enable | `0x00` – Disabled<br>`0x11` – Enabled | Yes | No |
| 11:6 | Secondary boot device | Indicates the device on which rest of the data is present in.<br>0 - Same boot device (default)<br>1- qspi32<br>2- qspi24<br>3- Nand<br>4- sd0<br>5- sd1<br>6- sd_ls<br>7- eMMC<br>8- USB<br>9- ethernet<br>0xa – PCIe<br>0xb- SATA<br>0xc- ospi<br>0xd- SMAP<br>0xe- sbi<br>0xf - sd0-raw<br>0x10 - sd1-raw<br>0x11 - sd-ls-raw<br>0x12 - mmc-raw<br>0x13 – mmc0<br>0x14 – mmc0-raw<br>0x15 – Image Store<br>0x16 - UFS<br>All other Reserved | Yes. | Yes |
| 5:1 | Image Creator | 0 – Unknown (Default)<br>1 – WDI<br>2 - SDK<br>Others - Reserved | Yes | Yes |
| 0 | Silicon Revision Code check | 0 – Silicon Revision in Verify ID code before loading PDI (default)<br>1 – Silicon Revision in ID code is not verified before loading PDI | Yes | Yes |

#### Optional Data

Optional data is binary data placed in the PDI after the image header table (IHT). You can include your own optional data like data, version, and so on. For more information, refer to optionaldata.
This data is authenticated as part IHT Signature and IHT is added as additional authenticated data (AAD) for the first secure header (SH) of the image header (IH) during encryption along with IHT. This implies that the optional data content remains as plain text.
As part of Authentication Optimization, for partitions that use the same authentication keys, the partition hashes are placed as part of Optional Data. For more information, see Authentication Optimization.
This was added to support optional metadata that is always in clear text and is not considered Image or Partition information. This is located immediately after the IHT. The size of this data is determined by the Size of IHT Optional Data (offset `0x58`) field in the IHT. The content is a list of the following data structure:

**Table: Optional Data**

| Offset | Name | Description |
| :--- | :--- | :--- |
| `0x0` | ID | The Data ID field identifies the type of data contained in the data structure.<br>0: None, can be used for padding<br>1: PLM Build Time configuration metadata as defined in the PLM Configuration document<br>2: Data structure version information used in In-Place PLM Update Compatibility Check<br>4: Reserved |
| `0x2` | Size | Total optional data size in words<br>The Size field specifies the number of words in the data structure.<br>The minimal value is 1 which indicates that no data or checksum is present. This is useful for padding.<br>Value 2 indicates that checksum is present, but no data. |
| `0x4` | Data | Actual data including padding |
| Last | Checksum | Sum of all the previous words in this data structure |

### Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Image Header

The Image Header contains metadata that identifies the boot image, including the type, size, and location. This information is crucial for the bootloader to correctly interpret and load the image. The header includes security-related information such as encryption keys and authentication data. This ensures that only verified and trusted images are loaded.
User can define multiple files to form an Image. Every image contains image header associated with it. User can define dependent systems under a single image. Bootgen creates required partitions for each file and create a common image header for that image. An image header points to the partitions (partition headers) that are associated with this image.

**Table: Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Image Header**

| Offset | Name | Description |
| :--- | :--- | :--- |
| `0x0` | First Partition Header | Word offset to first partition header |
| `0x4` | No of Partitions | Number of partitions present for this image |
| `0x8` | Meta header revocation ID | Revocation ID for meta header. If there are multiple IHs, then the same Revocation ID are placed into each IH. |
| `0xC` | Image Attributes | See Image Attributes table. |
| `0x10-0x1C` | Image Name | ASCII name of the image. Maximum 16 characters. |
| `0x20` | Image ID / Node ID | ID of the image or node. |
| `0x24` | Unique ID (UID) | Defines affinity and compatibility identifier when required for a particular device resource (for example, DFs binding to DP). For resources where the UID is not used, for example, power and subsystem resources, this field is ignored. |
| `0x28` | Parent UID | Defines required parent resource UID for the image’s configuration content, if required. For example, DFs compatible DP UID. |
| `0x2C` | Function ID | Identifier used to capture unique function of the image configuration data (for example, multiple DFs which all have compatibility to a given DP). |
| `0x30` | DDR memory low address | DDR memory address where image is copied when CopyToMem is enabled. |
| `0x34` | DDR memory high address | |
| `0x38 – 0x39` | PCR Number | PCR number to which the measurement are applied, valid numbers are 2 to 7. For any other value, Bootgen throws an error. |
| `0x3A – 0x3B` | Measurement Index | Measurement index order to extend the measurement.<br>The maximum number of indexes per PCR is fixed and is configured through a CDO command. Software throws an error if the measurement index is not within the limits. |
| `0x3C` | Checksum | A sum of all the previous words. |

### Versal AI Edge Gen 2 and Versal Prime Gen 2 Partition Header

The Partition Header specifies the start and end addresses of each partition within the boot image. This helps in accurately locating and loading the partitions during the boot process.

**Table: Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Partition Header**

| Offset | Name | Description |
| :--- | :--- | :--- |
| `0x0` | Partition Data Word Length | Encrypted partition data length |
| `0x4` | Extracted Data Word Length | Unencrypted data length |
| `0x8` | Total Partition Word Length (Includes Authentication Certificate) | The total encrypted + padding + expansion + authentication length |
| `0xC` | Next Partition header offset | Offset of next partition header |
| `0x10` | Destination Execution Address (Lower Half) | The executable address of this partition after loading. |
| `0x14` | Destination Execution Address (Higher Half) | The executable address of this partition after loading. |
| `0x18` | Destination Load Address (Lower Half) | The RAM address to load the partition. For elf files, Bootgen automatically reads from elf format. For RAW data. ensure to specify where to load it. For CFI and configuration data, it is `0xFFFF_FFFF`. |
| `0x1C` | Destination Load Address (Higher Half) | The RAM address to load the partition. For elf files, Bootgen automatically reads from elf format. For RAW data, ensure to specify where to load it. For CFI and configuration data, it is `0xFFFF_FFFF`. |
| `0x20` | Data Word Offset in Image | The position of the partition data relative to the start of the boot image. |
| `0x24` | Attribute Bits | See Partition Attributes. |
| `0x28` | Section Count | If image type is elf, it describes the number of partitions that are associated with this elf. |
| `0x2C` | Checksum Word Offset | The location of the checksum word in the boot image. |
| `0x30` | Partition ID | Partition ID |
| `0x34` | Hash block Authentication Certification Word Offset | The location of the hash block authentication certification in the boot image when authentication exists and is zero if the authentication is not enabled. |
| `0x38 – 0x40` | IV | IV for PHs Secure header |
| `0x44` | Encryption Key select | Encryption status:<br>`0x00000000` – Unencrypted<br>256-bit key sizes<br>`0xA5C3C5A3` - eFuse Key<br>`0xA5C3C5A5` - eFuse Black Key<br>`0xA5C3C5A7` - eFuse Obfuscated Key<br>`0x3A5C3C5A` - BBRAM Key<br>`0x3A5C3C59` - BBRAM Black Key<br>`0x3A5C3C57` - BBRAM Obfuscated Key<br>`0xA35C7C53` - Boot Header Black Key<br>`0xA35C7CA5` - Boot Header Obfuscated Key<br>128 and 256 bit key sizes<br>`0x5C3CA5A3` - eFuse User Key 0<br>`0x5C3CA5A5` - eFuse User key 0 Black<br>`0x5C3CA5A7` - eFuse User key 0 Obfuscated<br>`0xC3A5C5A3` - eFuse User Key 1<br>`0xC3A5C5A5` - eFuse User key 1 Black<br>`0xC3A5C5A7` - eFuse User key 1 Obfuscated<br>`0xC5C3A5A3` - User Key 0<br>`0xC3A5C5B3` - User Key 1<br>`0xC5C3A5C3` - User Key 2<br>`0xC3A5C5D3` - User Key 3<br>`0xC5C3A5E3` - User Key 4<br>`0xC3A5C5F3` - User Key 5<br>`0xC5C3A563` - User Key 6<br>`0xC3A5C573` - User Key 7 |
| `0x48` | IV for KEK decryption | IV for decrypting black or obfuscated key |
| `0x54` | Partition revocation ID | Revocation ID for partition which is valid in case of image is in encrypted format |
| `0x58` | Measured Boot Address | Single Byte measured boot address |
| `0x5C` | Authentication Header | Hash algorithm selection and authentication algorithm |
| `0x60` | Hash block length | Hash block length for Partition |
| `0x64` | Hash block offset | Partition hash block offset |
| `0x68` | Total PPK Size | Includes key alignment length in case, else actual PPK size |
| `0x6C` | Actual PPK Size | Actual PPK size |
| `0x70` | Total Hash block signature size | Hash block signature Size with padding to align |
| `0x74` | Actual Hash block signature size | Actual Hash block signature size |
| `0x78` | Reserved | 0 |
| `0x7C` | Header Checksum | A sum of the previous words in the Partition Header |

#### Partition Header Table Attribute

The following table lists the partition header table attributes.

**Table: Partition Header Table Attribute**

| Bit Field | Name | Description |
| :--- | :--- | :--- |
| 31:29 | Destination Cluster | 0 - A78 Cluster 0 (or) R52 Cluster 0<br>1 - A78 Cluster 1 (or) R52 Cluster 1<br>2 - A78 Cluster 2 (or) R52 Cluster 2<br>3 - A78 Cluster 3 (or) R52 Cluster 3<br>4- R52 Cluster 4 |
| 28:27 | DPA CM Enable | `0x00` – Disabled<br>`0x11` – Enabled |
| 26:24 | Partition Type | 0 – Reserved<br>1 - elf<br>2 - Configuration Data Object<br>3 - Cframe Data (PL data)<br>4 – Raw Data<br>5 – Raw elf<br>6 – CFI GSR CSC unmask frames<br>7 – CFI GSR CSC mask frames |
| 23 | HiVec | VInitHi setting for RPU/APU(32-bit) processor.<br>0 – LoVec<br>1 – HiVec |
| 22:21 | Reserved | 0 |
| 20:19 | Reserved | 0 |
| 18 | BE or LE | 0 – Little endian (Default)<br>1 – Big endian |
| 17:16 | Partition Owner | 0 - PLM (Default)<br>1 - Non-PLM<br>2,3 – Reserved |
| 15:14 | Reserved | |
| 13:12 | Checksum Type | 00b - No Checksum (Default)<br>11b – SHA3 |
| 11:8 | Destination CPU | 0 – None (Default for non-elf files)<br>1 - A78-0<br>2 - A78-1<br>3 – A78-2<br>4 – A78-3<br>5 - R52-0<br>6 - R52-1<br>7- Reserved<br>8 – ASU<br>9 – AIE<br>10-15 – Reserved |
| 7:6 | Reserved | |
| 5:4 | Cluster Lockstep | `0x0` – Lockstep Disabled<br>`0x3` – Lockstep Enabled |
| 3 | A78 CPU execution state | 0 - Aarch64 (default)<br>1 - Aarch32 |
| 2:1 | EL level configuration for A78 core | 00b – EL0<br>01b – EL1<br>10b – EL2<br>11b – EL3 (default) |
| 0 | TZ secure partition | 0 – Non-Secure<br>1 – Secure (Default)<br>This bit indicates if the PLM is required to configure the core (on which this partition executes). Ensure to configure as TZ secure or not. Default value is 0. |

### Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Authentication Certificate

The authentication certificate is a structure that contains all the information related to the authentication of a partition. This structure contains public keys and the signatures that BootROM/PLM requires verification. There is an authentication header in each authentication certificate, which provides information for the key sizes and algorithm used for signing. Similar to Versal adaptive SoC, the authentication certificate is attached to the beginning of the actual partition PLM/PL data, for which authentication is enabled. For Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 along with RSA and ECDSA-P384, it also supports HSS-SHAKE256 and LMS-SHAKE256.
The following tables provide the format of the authentication certificates.

**Table: Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Authentication Certificate – ECDSA P-384**

| Authentication Certificate Bits | Description |
| :--- | :--- |
| `0x00` | PPK<br>x (48 bytes) – (coordinate )<br>y (48 bytes) – (coordinate) |
| `0x60` | Total SPK size |
| `0x64` | Actual SPK size |
| `0x68` | Total SPK signature size |
| `0x6C` | Actual SPK signature size |
| `0x70` | SPK Revocation ID |
| `0x74` | SPK header alignment |
| `0x80` | SPK<br>x (48 bytes) – (coordinate )<br>y (48 bytes) – (coordinate) |
| `0xE0` | SPK signature |

**Table: Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Authentication Certificate – LMS and HSS**

| Authentication Certificate Bits | Description |
| :--- | :--- |
| `0x00` | PPK |
| `0x3c` | PPK alignment |
| `0x40` | Total SPK size |
| `0x44` | Actual SPK size |
| `0x48` | Total SPK sign size |
| `0x4C` | Actual SPK sign size |
| `0x50` | SPK Revocation ID |
| `0x54` | SPK header alignment |
| `0x60` | SPK |
| `0x9C` | SPK align |
| `0xA0` | SPK sign |

> ✎ **Note:** Refer to LMS Key Expiration Error for more details.

#### Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Authentication Header

This header defines details of boot Authentication scheme for Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 PDI.

**Table: Versal AI Edge Series Gen 2 and Versal Prime Series Gen 2 Authentication Header**

| Bits | Size | Field | Description |
| :--- | :--- | :--- | :--- |
| [31:18] | 14 | Reserved | Reserved |
| [17:16] | 2 | PPK Key Sel | • 0: PPK 0 Selection in Efuse<br>• 1: PPK 1 Selection in Efuse<br>• 2: PPK 2 Selection in Efuse<br>• 3: Rsvd |
| [15:14] | 2 | Authentication Certificate Format | Reserved |
| [13:12] | 2 | Authentication Certificate Version | 00: Current AC |
| [11] | 1 | PPK KeyType | 0: Hash Key |
| [10:9] | 2 | PPK Key Src | 0: eFUSE |
| [8] | 1 | SPK Enable | 1: SPK Enabled |
| [7:4] | 4 | HASH Algorithm | • 0: SHA2-384 (not supported)<br>• 1: SHA3/384<br>• 2 to 16: Reserved for future use |
| [3:0] | 4 | Public Algorithm | • 0: Non-Secure<br>• 1: RSA4096<br>• 2: ECDSAp384<br>• 3: ECDSAp521 (only for PLM loadable partitions)<br>• 4: LMS with HSS<br>• 5: LMS<br>• 6 to 16: Reserved for future |

---

## Creating Boot Images

### Boot Image Format (BIF)

The AMD boot image layout has multiple files, file types, and supporting headers to parse those files by boot loaders. Bootgen defines multiple attributes for generating the boot images and interprets and generates the boot images, based on what is passed in the files. Because there are multiple commands and attributes available, Bootgen defines a boot image format (BIF) to contain those inputs. A BIF comprises of the following:

*   Configuration attributes to create secure/liveness sets and performs basic block traversals. These traversals can be avoided if def-use chains are available.

Recently, Boissinot et al. [16] proposed a fast liveness check for SSA that answers whether a given variable is live at a given program location. This technique does not require liveness sets but relies on pre-computed data-structures that only depend on the control flow graph. These data-structures are thus still valid even if instructions are moved, introduced, or removed. For testing if two SSA variables interfere, it is enough to check if one variable is live at the definition of the other.

We will not detail these different intersection tests any further. In the next section, they are used as a black box for developing an algorithm that checks interference between two congruence classes with a linear number of interference tests.

### B. Linear interference test between two congruence classes (with extension to value-based interferences)

Sections III-A and III-C presented two of our main contributions, the notion of value-based interference and the method to sequentialize parallel copies. We now present our third main contribution: how to efficiently perform an interference test between two sets of already-coalesced variables (congruence classes in Sreedhar et al. terminology). Suppose that the two tests needed to decide if two SSA variables interfere – the live range intersection test (Section IV-A) and the “has-the-same-value” test (Section III-A) – are available as black boxes. To replace the quadratic number of tests by a linear number of tests, we simplify and generalize the dominance-forest technique proposed by Budimlić et al. [15]. Our contributions are: a) we avoid constructing explicitly the dominance forest; b) we are also able to check for interference between *two* sets; c) we extend this check to support value-based interferences.

Given a set of variables, Budimlić et al. define its *dominance forest* as a graph forest where the ancestors of a variable are exactly the variables of the set that dominate it (i.e., whose definition point dominates the definition point of the other). The key idea of their algorithm is that the set contains two intersecting variables if and only if it contains a variable that intersects with its parent in the dominance forest. So they just traverse the dominance forest and check the live range intersection for each of its edges. Instead of constructing explicitly the dominance forest, we propose to represent each congruence class as a list of variables ordered according to a pre-DFS order $\prec$ of the dominance tree (i.e., a depth-first search where each node is ordered before its successors). Then, because querying if a variable is an ancestor of another one can be achieved in $O(1)$ (a simple dominance test), simulating the stack of a recursive traversal of the dominance forest is straightforward. Thus, as in [15], we can derive a linear-time intersection test for a set of variables (**Algorithm 2**).

Now consider two intersection-free sets (two congruence classes of non-intersecting variables) `blue` and `red`. To coalesce them, there should be no intersection between any two variables. We proceed as if the two sets were merged and apply the previous technique. The only difference is that we omit the intersection tests if two variables are in the same set: in Line 10 of Algorithm 2, the `intersect` query should check if `parent` and `current` belong to a different list.

```text
Algorithm 2: Check intersection in a set of variables
Data: List of variables list sorted according to a
      pre-DFS order of the dominance tree
Output: Returns true if the list contains an interference
1  dom ← empty_stack ;            /* stack of the traversal */
2  i ← 0 ;
3  while i < list.size() do
4    current ← list(i++) ;
5    other ← dom.top() ;          /* ⊥ if dom is empty */
6    while (other ≠ ⊥) and dominate(other, current) = false do
7      dom.pop() ; /* not the desired parent, remove */
8      other ← dom.top() ;        /* consider next one */
9    parent ← other ;
10   if (parent ≠ ⊥) and (intersect(current, parent) = true)
         then return true ;       /* intersection detected */
11   dom.push(current) ;          /* otherwise, keep checking */
12 return false ;
```

Also, because each set is represented as an ordered list, traversing two lists in order is straightforward. We just progress in the right list, according to the pre-DFS order $\prec$ of the dominance tree. We use two indices $i_r$ and $i_b$ and replace Lines 2-4 of the previous algorithm by the following lines:

```text
ir ← 0 ; ib ← 0 ;
while ir < red.size() or ib < blue.size() do
  if ir = red.size() or (ir < red.size() and ib < blue.size()
  and blue(ib) ≺ red(ir)) then current ← blue(ib++) ;
  else current ← red(ir++) ;
```

The last refinement is to extend our intersection technique to an interference test that accounts for value equalities. Suppose that $b$ is the parent of $a$ in the dominance forest. In the previous algorithm, the induction hypothesis is that the subset of already-visited variables is intersection-free. Then, if $c$ is an already-visited variable, the fact that $b$ and $a$ do not intersect guarantees that $c$ and $a$ do not intersect, otherwise the intersection of $b$ and $c$ would have already been detected. However, for interferences with value equalities, this is no longer true. The variable $c$ may intersect $b$ but if they have the same value, they do not interfere. The consequence is that, now, $a$ and $c$ may intersect even if $a$ and $b$ do not intersect. However, if $a$ does not intersect $b$ and any of the variables it intersects, then $a$ does not intersect any of the already-visited variables. To speed up such a test and to avoid checking intersection between variables in the same set, we keep track of one additional information: for each variable $a$, we store the nearest ancestor of $a$ that has the same value and that intersects it. We call it the “equal intersecting ancestor” of $a$. We assume that the equal intersecting ancestor is pre-computed within each set, denoted by `equal_anc_in(a)`, and we compute the equal intersecting ancestor in the opposite set, denoted by `equal_anc_out(a)`. The skeleton of the algorithm for interference test with value equalities is the same as for Algorithm 2, with the patch to progress along the lists `red` and `blue`, and where the call Line 10 is now an **interference** test (Function `interference`). The principle of the algorithm is apparent in the pseudo-code. Two equal intersecting ancestors, `in` and `out`, are used to make sure that the test `intersect(a, b)`, which runs a possibly expensive intersection test, is performed only if $a$ and $b$ belong to different sets.

```text
Function update_equal_anc_out(a, b)
Data: Variables a and b, same value, but in different sets
Output: Set nearest intersecting ancestor of a, in other
        set, with same value (⊥ if does not exist)
1 tmp ← b ;
2 while (tmp ≠ ⊥) and (intersect(a, tmp) = false) do
3   tmp ← equal_anc_in(tmp) ; /* follow the chain of
      equal intersecting ancestors in the other set */
4 equal_anc_out(a) ← tmp ; /* tmp intersects a or ⊥ */

Function chain_intersect(a, b)
Data: Variables a and b, different value, in different sets
Output: Returns true if a intersects b or one of its equal
        intersecting ancestors in the same set
1 tmp ← b ;
2 while (tmp ≠ ⊥) and (intersect(a, tmp) = false) do
3   tmp ← equal_anc_in(tmp) ; /* follow the chain of
      equal intersecting ancestors */
4 if tmp = ⊥ then return false else return true ;

Function interference(a, b)
Data: A variable a and its parent b in the dominance tree
Output: Returns true if a interferes (i.e., intersects and
        has a different value) with an already-visited
        variable. Also, update equal_anc information
/* a and b are assumed to not be equal to ⊥ */
1 equal_anc_out(a) ← ⊥ ;                  /* initialization */
2 if a and b are in the same set then
3   b ← equal_anc_out(b) ; /* check/update in other set */
4 if value(a) ≠ value(b) then
5   return chain_intersect(a, b) ; /* check with b and its
      equal intersecting ancestors in the other set */
6 else
7   update_equal_anc_out(a, b) ;          /* update equal
      intersecting ancestor going up in the other set */
8   return false ;                        /* no interference */
```

Note that once a list is empty and the stack does not contain any element of this list, there is no more intersection or updates to make. Thus, the algorithm should be stopped, i.e., the while loop condition in Algorithm 2 can be replaced by:

```text
while (ir < red.size() and nb > 0) or (ib < blue.size() and
nr > 0) or (ir < red.size() and ib < blue.size()) do
```

where $n_r$ (resp. $n_b$) are variables that count the number of stack elements that come from the list `red` (resp. `blue`). Finally, in case of coalescing, the two lists are merged into a unique ordered list (takes linear time, using a similar joined traversal), while the equal intersecting ancestor `equal_anc_in(a)` for the combined set is updated to the maximum (following the pre-DFS order $\prec$) of `equal_anc_in(a)` and `equal_anc_out(a)`.

### C. Virtualization of the φ-nodes

Implementation of the whole procedure, as described in Section III, starts by introducing many new variables $a'_i$ (one for each argument of a φ-function, plus its result) and copies in the basic block of the φ-function and in its predecessors. These variables are immediately coalesced together, into what we call a φ-node, and stored into a congruence class. Nevertheless, in the data structures used (interference graph, liveness sets, variable name universe, parallel copy instructions, congruence classes), these variables exist and consume memory and time, even if at the end, after coalescing, they may disappear.

To avoid the introduction of these initial variables and copies, our inspiration comes from the Method III of Sreedhar et al., which emulates the whole process and introduces copies on the fly, only when they appear to be required. We want our implementation to be clean and able to handle all the special cases without tricks. For that purpose, we use exactly the same algorithms as for the solution without virtualization. We use a special location in the code, identified as a “virtual” parallel copy, where the real copies, if any, will be placed. The original arguments (resp. results) of a φ-function are then assumed, initially, to have a “use” (resp. “def”) in the parallel copy but are not considered as live-out (resp. live-in) along the corresponding control flow edge. Then, the algorithm selects the copies to coalesce, following some order, either a real copy or a virtual copy. If it turns out that a virtual copy $a_i \to a'_i$ (resp. $a'_0 \to a_0$) cannot be coalesced, it is materialized in the parallel copy and $a'_i$ (resp. $a'_0$) becomes explicit in its congruence class. The corresponding φ-operand is replaced and the use of $a'_i$ (resp. def of $a'_0$) is now assumed to be on the corresponding control flow edge. This way, only copies that the first approach would finally leave uncoalesced are introduced.

The key point to make the emulation of copy insertion possible is that one should never have to test an interference with a variable that is not yet materialized or coalesced. For that reason, φ-functions are treated one by one, and all virtual copies that imply a variable of the φ-function are considered (either coalesced or materialized) before examining any other copy. The weakness of this approach is that a global coalescing algorithm cannot be used because only a partial view of the interference structure is available to the algorithm. However, the algorithm can still be guided by the weight of copies, i.e., the dynamic count associated to the block where it would be placed if not coalesced. The rest is only a matter of accurate implementation, but once again intrinsically this is nothing else than emulating these copies and variables.

> **Figure 6:** Performance results in terms of speed (time to go out of SSA). (Bar chart showing variants: Sreedhar III, Us III, Us III + InterCheck, Us III + InterCheck + LiveCheck, Us III + Linear + InterCheck + LiveCheck, Us I, Us I + Linear + InterCheck + LiveCheck across various SPEC CINT2000 benchmarks).

> **Figure 7:** Performance results in terms of memory footprint (maximum and total). (Bar charts showing Measured, Evaluated (Ordered sets), and Evaluated (Bit sets) for the same variants as Figure 6).

### D. Results in terms of speed and memory footprint

To measure the potential of our different contributions, in terms of speed-up and memory footprint reduction, we implemented a generic out-of-SSA translation that enables to evaluate different combinations. We selected the following:

*   **`Us I`** Simple coalescing with no virtualization, but different techniques for checking interferences and liveness.
*   **`Sreedhar III`** Method III of Sreedhar et al. (thus with virtualization) complemented by their SSA-based coalescing for non φ-related copies. Both use an interference graph stored as a bit-matrix and liveness sets as ordered sets.
*   **`Us III`** Our implementation of virtualization of φ-related copies followed by coalescing of other copies. This implementation is generic enough to support various options: with parallel or sequential copies, with/without interference graph, with/without liveness sets. Hence, its implementation is less tuned than `Sreedhar III`.

By default, `Us III` and `Us I` use an interference graph and classic liveness sets. The options are:

*   **`InterCheck`** No interference graph: intersections are checked using dominance and the liveness sets as in [15].
*   **`InterCheck+LiveCheck`** No interference graph and no liveness sets: intersections are checked with the fast liveness checking algorithm of [16], see Section IV-A.
*   **`Linear+InterCheck+LiveCheck`** In addition, our linear intersection check is used instead of the quadratic one.

When an interference graph, liveness sets, or liveness checking are used, timings include their construction. Figure 6 shows the timings for these different variants versus `Sreedhar III` as a baseline. `InterCheck` always slows down the execution, while `LiveCheck` and `Linear` always speedup the execution by a significant ratio. A very interesting result is that the simple SSA-based coalescing algorithm without any virtualization is as fast as the complex algorithm with virtualization. Indeed, when using `Linear+InterCheck+LiveCheck`, adding first all copies and corresponding variables before coalescing them, does not have the negative impact measured by Sreedhar et al. any longer. Hence `Us I+Linear+InterCheck+LiveCheck` provides a quite attractive solution, which is about twice faster than `Sreedhar III`. Also, thanks to our interference definition with equality of values, the quality (in terms of copies) of the generated code does not depend on the virtualization, unlike in methods by Sreedhar et al.

Figure 7 shows the memory footprint used for the interference graph and the liveness sets. The variable universe used for liveness and interference information is restricted to the φ-related and copy-related variables.

*   **Interference graph** is stored using a half-size bit-matrix. `Measured` provides the measured footprint from the statistics provided by our memory allocator. In `Sreedhar III` or `Us III`, variables are added incrementally so the bit-matrix grows dynamically. This leads to a memory footprint slightly higher than for a perfect memory. The behavior of such a perfect memory is evaluated in `Evaluated` using the formula $\lceil \text{\#variables} / 8 \rceil \times \text{\#variables} / 2$.
*   **Liveness sets** are stored as ordered sets. `Measured` provides the measured footprint of the liveness sets, without counting those used in liveness construction. As for the interference graph, liveness sets are modified by `Sreedhar III` or `Us III`. Since the number of simultaneous live variables does not change, their sizes remain roughly the same. Because the use of ordered sets instead of bit-sets is arguable, we evaluated the corresponding footprint of liveness sets, for a perfect memory, by counting the size of each set. For bit-sets, we evaluated the footprint using the formula $\lceil \text{\#variables} / 8 \rceil \times \text{\#basicblocks} \times 2$.
*   **Liveness checking** uses two bit-sets per basic block, plus a few other sets during construction. These sets are measured in the memory footprint. A perfect memory is evaluated using the formula $\lceil \text{\#basicblock} / 8 \rceil \times \text{\#basicblock} \times 2$.

The results show that the main gain comes from the removal of the interference graph. We point out that the memory used for liveness sets construction is difficult to optimize and might lead to a very large memory footprint in practice. On the other hand, the liveness checking data structures depend only on the control flow graph. Our statistics favor the classic liveness sets, as the memory usage for their construction has been omitted, while the memory usage for the liveness checking has been kept. As an illustration, in our compiler, the memory footprint for the liveness sets construction is of the same order of magnitude as for the interference graph construction.

In conclusion, `Us I+Linear+InterCheck+LiveCheck` is a simple and clean solution, as it avoids the complexity of the implementation of virtualization. Yet it leads to a memory footprint about 10 times smaller than `Sreedhar III`.

## V. Conclusions

We revisited the out-of-SSA translation techniques for the purposes of ensuring correctness, quality of generated code, and efficiency (speed and memory footprint) of implementation. This work is motivated by the use of the SSA form in JIT compilers for embedded processors. The techniques proposed by Sreedhar et al. [3] fix the correctness issues of previous algorithms, allow critical edges in the control flow graph, and produce code of good quality. However, their optimized version (Method III) is hard to implement correctly when dealing with branch instructions that use or define variables. The technique proposed by Budimlić et al. [15] is geared towards speed, as it relies on dominance for the fast intersection of SSA live ranges and introduces dominance forests for finding intersections in a set of SSA variables in linear time. This technique does not allow critical edges in the control flow graph and is difficult to implement correctly. Still, the idea to optimistically coalesce variables with a rough but cheap filtering, then decoalesce interfering variables within the obtained congruence classes, is interesting. This coalescing scheme is orthogonal to and compatible with our techniques.

We significantly advanced the understanding of out-of-SSA translation by reformulating it as an aggressive coalescing problem applied to the CSSA program resulting from Method I of Sreedhar et al. Our key insight, supported by our experiments, is that interferences must be considered as intersection refined with value equivalence for any out-of-SSA translation to be effective. Thanks to the SSA structural properties, computing the value equivalence comes at no cost. This leads to a solution that is provably-correct, generic, easy to implement, and that can benefit from register allocation techniques. In particular, our implementation also coalesces copies inserted before the out-of-SSA translation to satisfy register renaming constraints (dedicated registers, calling conventions, etc.).

Then, we generalized the idea of dominance forests of Budimlić et al., first to enable interference checking between two congruence classes, then to take into account the equality of values. In addition, our implementation is much simpler as we do not explicitly build the dominance forest. The reduced number of SSA variable intersection tests that results from this technique enables more expensive intersection checks that do not rely on liveness sets or explicit interference graph.

Last, we developed a solution, similar to Method III of Sreedhar et al., for the virtualization of the φ-related copies, i.e., to introduce variables and copies on the fly, only when their insertion is decided. Surprisingly, unlike for Sreedhar et al. methods, our experiments performed on the SPEC CINT2000 benchmarks show that virtualization, which is hard to implement, does not bring any clear benefit in terms of speed and memory consumption. This is because, thanks to fast liveness checking [16] and our linear-complexity intersection test, we do not need any interference graph or liveness sets. Also, with value-based interference, virtualization is equivalent in terms of code quality, in other words, inserting all copies first does not degrade coalescing. Our out-of-SSA translation algorithm, without virtualization, outperforms the speed of Method III of Sreedhar et al. by a factor of 2, reduces the memory footprint by a factor of 10, while ensuring comparable or better copy coalescing abilities. However, we point out that, so far, we handle register renaming constraints with explicit copy insertions. It is possible that virtualization of such copy insertions is useful. This is left for future work.

## References

[1] R. Cytron, J. Ferrante, B. K. Rosen, M. N. Wegman, and F. K. Zadeck, “Efficiently computing static single assignment form and the control dependence graph,” *ACM Transactions on Programming Languages and Systems*, vol. 13, no. 4, pp. 451 – 490, 1991.
[2] A. W. Appel and J. Palsberg, *Modern Compiler Implementation in Java*, 2nd ed. Cambridge University Press, 2002.
[3] V. C. Sreedhar, R. D.-C. Ju, D. M. Gillies, and V. Santhanam, “Translating out of static single assignment form,” in *Static Analysis Symposium (SAS’99)*, Italy, 1999, pp. 194 – 204.
[4] A.-R. Adl-Tabatabai, M. Cierniak, G.-Y. Lueh, V. M. Parikh, and J. M. Stichnoth, “Fast, effective code generation in a just-in-time java compiler,” in *International Conference on Programming Language Design and Implementation (PLDI’98)*. ACM Press, 1998, pp. 280–290.
[5] M. Poletto and V. Sarkar, “Linear scan register allocation,” *ACM Transactions on Programming Languages and Systems*, vol. 21, no. 5, pp. 895–913, 1999.
[6] O. Traub, G. Holloway, and M. D. Smith, “Quality and speed in linear-scan register allocation,” in *Int. Conf. on Programming Language Design and Implementation (PLDI’98)*. ACM Press, 1998, pp. 142–151.
[7] C. Wimmer and H. Mössenböck, “Optimized interval splitting in a linear scan register allocator,” in *ACM/USENIX International Conference on Virtual Execution Environments (VEE’05)*. Chicago, IL, USA: ACM, 2005, pp. 132–141.
[8] V. Sarkar and R. Barik, “Extended linear scan: An alternate foundation for global register allocation,” in *International Conference on Compiler Construction (CC’07)*, ser. LNCS, vol. 4420. Braga, Portugal: Springer Verlag, Mar. 2007, pp. 141–155.
[9] B. Dupont de Dinechin, “Inter-block scoreboard scheduling in a JIT compiler for VLIW processors,” in *Euro-Par 2008 - Parallel Processing, 14th International Euro-Par Conference*, ser. LNCS, vol. 5168. Las Palmas de Gran Canaria, Spain: Springer, Aug. 2008, pp. 370–381.
[10] J. Cavazos and J. E. B. Moss, “Inducing heuristics to decide whether to schedule,” in *International Conference on Programming Language Design and Implementation (PLDI’04)*. Washington, DC, USA: ACM Press, 2004, pp. 183–194.
[11] V. Tang, J. Siu, A. Vasilevskiy, and M. Mitran, “A framework for reducing instruction scheduling overhead in dynamic compilers,” in *Conference of the Center for Advanced Studies on Collaborative Research (CASCON’06)*. Toronto, Ontario, Canada: ACM, 2006, p. 5.
[12] P. Briggs, K. D. Cooper, T. J. Harvey, and L. T. Simpson, “Practical improvements to the construction and destruction of static single assignment form,” *Software – Practice and Experience*, vol. 28, no. 8, pp. 859–881, Jul. 1998.
[13] V. C. Sreedhar and G. R. Gao, “A linear time algorithm for placing φ-nodes,” in *22nd ACM SIGPLAN-SIGACT Symposium on Principles of Programming Languages (POPL’95)*. ACM, 1995, pp. 62–73.
[14] A. Gal, C. W. Probst, and M. Franz, “Structural encoding of static single assignment form,” *Electronic Notes in Theoretical Computer Science*, vol. 141, no. 2, pp. 85–102, dec 2005.
[15] Z. Budimlić, K. D. Cooper, T. J. Harvey, K. Kennedy, T. S. Oberg, and S. W. Reeves, “Fast copy coalescing and live-range identification,” in *International Conference on Programming Language Design and Implementation (PLDI’02)*. ACM Press, June 2002, pp. 25–32.
[16] B. Boissinot, S. Hack, D. Grund, B. D. de Dinechin, and F. Rastello, “Fast liveness checking for SSA-form programs,” in *Int. Symp. on Code Generation and Optimization (CGO’08)*. IEEE/ACM, 2008, pp. 35–44.
[17] G. J. Chaitin, M. A. Auslander, A. K. Chandra, J. Cocke, M. E. Hopkins, and P. W. Markstein, “Register allocation via coloring,” *Computer Languages*, vol. 6, pp. 47–57, Jan. 1981.
[18] A. Leung and L. George, “Static single assignment form for machine code,” in *International Conference on Programming Language Design and Implementation (PLDI’99)*. ACM Press, 1999, pp. 204–214. [Online]. Available: citeseer.ist.psu.edu/leung99static.html
[19] F. Rastello, F. de Ferrière, and C. Guillon, “Optimizing translation out of SSA using renaming constraints,” in *International Symposium on Code Generation and Optimization (CGO’04)*. IEEE Computer Society Press, 2004, pp. 265–278.
[20] F. Bouchez, A. Darte, and F. Rastello, “On the complexity of register coalescing,” in *International Symposium on Code Generation and Optimization (CGO’07)*. IEEE Computer Society Press, Mar. 2007, pp. 102–114.
[21] L. George and A. W. Appel, “Iterated register coalescing,” *ACM Transactions on Programming Languages and Systems*, vol. 18, no. 3, May 1996.
[22] M. D. Smith, N. Ramsey, and G. Holloway, “A generalized algorithm for graph-coloring register allocation,” in *International Conference on Programming Language Design and Implementation (PLDI’04)*. ACM, 2004, pp. 277–288.
[23] G. J. Chaitin, “Register allocation & spilling via graph coloring,” in *SIGPLAN Symp. on Compiler Construction (CC’82)*, 1982, pp. 98–101.
[24] B. Alpern, M. N. Wegman, and F. K. Zadeck, “Detecting equality of variables in programs,” in *15th Symposium on Principles of Programming Languages (POPL’88)*. ACM, 1988, pp. 1–11.
[25] F. Bouchez, A. Darte, C. Guillon, and F. Rastello, “Register allocation and spill complexity under SSA,” LIP, ENS-Lyon, France, Tech. Rep. RR2005-33, Aug. 2005.
[26] B. Dupont de Dinechin, F. de Ferrière, C. Guillon, and A. Stoutchinin, “Code generator optimizations for the ST120 DSP-MCU core,” in *International Conference on Compilers, Architecture, and Synthesis for Embedded Systems (CASES’00)*, 2000, pp. 93 – 103.
[27] P. Briggs, K. D. Cooper, and L. Torczon, “Improvements to graph coloring register allocation,” *ACM Transactions on Programming Languages and Systems*, vol. 16, no. 3, pp. 428–455, 1994.
[28] P. Briggs, K. D. Cooper, and L. T. Simpson, “Value numbering,” *Software – Practice and Experience*, vol. 27, no. 6, pp. 701–724, 1997.
[29] C. May, “The parallel assignment problem redefined,” *IEEE Transactions on Software Engineering*, vol. 15, no. 6, pp. 821–824, Jun. 1989.
