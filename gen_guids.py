#!/usr/bin/python3

import argparse
import os
import re

if __name__ == '__main__':
    parser = argparse.ArgumentParser("gen_guids")
    parser.add_argument("--verbose", help="Print GUIDs as they are processed.", action='store_true')
    parser.set_defaults(verbose=False)

    args = parser.parse_args()

    with open('guids.c', 'w') as guids_file:
        guids_file.write(
"""/*
 * AUTO GENERATED FILE
 *
 * Just a quick way to generate all the guids that EDK2 defines
 */
 
#include <Uefi.h>
 
""")

        guid_name_fixes = {
            'EFI_VT100_GUID': 'EFI_VT_100_GUID',
            'EFI_VT100_PLUS_GUID': 'EFI_VT_100_PLUS_GUID',
            'EFI_VTUTF8_GUID': 'EFI_VT_UTF8_GUID',
            'EFI_UART_DEVICE_PATH_GUID': 'DEVICE_PATH_MESSAGING_UART_FLOW_CONTROL',
            'EFI_SIMPLE_TEXT_IN_PROTOCOL_GUID': 'EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID',
            'EFI_SIMPLE_TEXT_OUT_PROTOCOL_GUID': 'EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID',
            'EFI_ACPI10_TABLE_GUID': 'EFI_ACPI_TABLE_GUID',
            'EFI_ACPI20_TABLE_GUID': 'EFI_ACPI_20_TABLE_GUID',
            'EFI_FILE_INFO_GUID': 'EFI_FILE_INFO_ID',
            'EFI_EVENT_EXIT_BOOT_SERVICES_GUID': 'EFI_EVENT_GROUP_EXIT_BOOT_SERVICES',
            'EFI_EVENT_VIRTUAL_ADDRESS_CHANGE_GUID': 'EFI_EVENT_GROUP_VIRTUAL_ADDRESS_CHANGE',
            'EFI_EVENT_MEMORY_MAP_CHANGE_GUID': 'EFI_EVENT_GROUP_MEMORY_MAP_CHANGE',
            'EFI_EVENT_READY_TO_BOOT_GUID': 'EFI_EVENT_GROUP_READY_TO_BOOT',
            'EFI_EVENT_DXE_DISPATCH_GUID': 'EFI_EVENT_GROUP_DXE_DISPATCH_GUID',
            'EFI_FILE_SYSTEM_INFO_GUID': 'EFI_FILE_SYSTEM_INFO_ID',
            'EFI_FILE_SYSTEM_VOLUME_LABEL_INFO_ID_GUID': 'EFI_FILE_SYSTEM_VOLUME_LABEL_ID',
            'EFI_GLOBAL_VARIABLE_GUID': 'EFI_GLOBAL_VARIABLE',
            'EFI_PART_TYPE_SYSTEM_PART_GUID': 'EFI_PART_TYPE_EFI_SYSTEM_PART_GUID',
            'EFI_UNICODE_COLLATION2_PROTOCOL_GUID': 'EFI_UNICODE_COLLATION_PROTOCOL2_GUID',
            'EFI_DEBUG_PORT_PROTOCOL_GUID': 'EFI_DEBUGPORT_PROTOCOL_GUID',
            'EFI_DEBUG_PORT_VARIABLE_GUID': 'EFI_DEBUGPORT_VARIABLE_GUID',
            'EFI_DEBUG_PORT_DEVICE_PATH_gGdtPtrGUID': 'DEVICE_PATH_MESSAGING_DEBUGPORT',
            'EFI_MP_SERVICE_PROTOCOL_GUID': 'EFI_MP_SERVICES_PROTOCOL_GUID',
            'EFI_MEMORY_OVERWRITE_REQUEST_CONTROL_LOCK_GUID': 'MEMORY_OVERWRITE_REQUEST_CONTROL_LOCK_GUID',
            'EFI_LEGACY_SPI_CONTROLLER_PROTOCOL_GUID': 'EFI_LEGACY_SPI_CONTROLLER_GUID',
            'EFI_WI_FI2_PROTOCOL_GUID': 'EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL_GUID',
            'EFI_PADDING_RSASSA_PKCS1_V1_P5_GUID': 'EFI_PADDING_RSASSA_PKCS1V1P5_GUID',
            'EFI_PADDING_RSAES_PKCS1_V1_P5_GUID': 'EFI_PADDING_RSAES_PKCS1V1P5_GUID',
            'EFI_WI_FI_PROTOCOL_GUID': 'EFI_WIRELESS_MAC_CONNECTION_PROTOCOL_GUID',
            'EFI_IP_SEC_PROTOCOL_GUID': 'EFI_IPSEC_PROTOCOL_GUID',
            'EFI_IP_SEC2_PROTOCOL_GUID': 'EFI_IPSEC2_PROTOCOL_GUID',
            'EFI_UFS_DEVICE_CONFIG_PROTOCOL_GUID': 'EFI_UFS_DEVICE_CONFIG_GUID',
            'EFI_TR_EE_PROTOCOL_GUID': 'EFI_TREE_PROTOCOL_GUID',
            'EFI_I_SCSI_INITIATOR_NAME_PROTOCOL_GUID': 'EFI_ISCSI_INITIATOR_NAME_PROTOCOL_GUID',
            'EFI_PCI_OVERRIDE_PROTOCOL_GUID': 'EFI_PCI_OVERRIDE_GUID',
            'EFI_MEMORY_OVERWRITE_REQUEST_CONTROL_LOCK_GUID': 'MEMORY_OVERWRITE_REQUEST_CONTROL_LOCK_GUID',
            'EFI_LEGACY_SPI_CONTROLLER_PROTOCOL_GUID': 'EFI_LEGACY_SPI_CONTROLLER_GUID',
            'EFI_WI_FI2_PROTOCOL_GUID': 'EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL_GUID',
            'EFI_PADDING_RSASSA_PKCS1_V1_P5_GUID': 'EFI_PADDING_RSASSA_PSS_GUID',
            'EFI_PADDING_RSAES_PKCS1_V1_P5_GUID': 'EFI_PADDING_RSAES_PKCS1V1P5_GUID',
            'EFI_PCI_ENUMERATION_COMPLETE_PROTOCOL_GUID': 'EFI_PCI_ENUMERATION_COMPLETE_GUID',
            'EFI_USB_FUNCTION_IO_PROTOCOL_GUID': 'EFI_USBFN_IO_PROTOCOL_GUID',
            'EFI_PCI_HOT_PLUG_REQUEST_PROTOCOL_GUID': 'EFI_PCI_HOTPLUG_REQUEST_PROTOCOL_GUID',
            'EFI_IP_SEC_CONFIG_PROTOCOL_GUID': 'EFI_IPSEC_CONFIG_PROTOCOL_GUID',
            'EFI_REGEX_SYNTAX_TYPE_ECMA262_GUID': 'EFI_REGEX_SYNTAX_TYPE_ECMA_262_GUID',
            'EFI_MDE_PKG_TOKEN_SPACE_GUID': 'MDEPKG_TOKEN_SPACE_GUID',
            'EFI_MEMORY_OVERWRITE_CONTROL_DATA_GUID': 'MEMORY_ONLY_RESET_CONTROL_GUID',
            'EFI_FMP_CAPSULE_GUID': 'EFI_FIRMWARE_MANAGEMENT_CAPSULE_ID_GUID',
            'APRIORI_GUID': 'EFI_APRIORI_GUID',
            'EFI_DXE_SERVICES_TABLE_GUID': 'DXE_SERVICES_TABLE_GUID',
            'EFI_HOB_LIST_GUID': 'HOB_LIST_GUID',
            'EFI_GRAPHICS_DEVICE_INFO_HOB_GUID': 'EFI_PEI_GRAPHICS_DEVICE_INFO_HOB_GUID',
            'EFI_FIRMWARE_VOLUME_TOP_FILE_GUID': 'EFI_FFS_VOLUME_TOP_FILE_GUID',
            'EFI_HII_KEY_BOARD_LAYOUT_GUID': 'EFI_HII_SET_KEYBOARD_LAYOUT_EVENT_GUID',
            'EFI_SMBIOS_TABLE_GUID': 'SMBIOS_TABLE_GUID',
            'EFI_SMBIOS3_TABLE_GUID': 'SMBIOS3_TABLE_GUID',
            'EFI_CERT_PKCS7_GUID': 'EFI_CERT_TYPE_PKCS7_GUID',
            'EFI_VECTOR_HANDOFF_TABLE_GUID': 'EFI_VECTOR_HANDOF_TABLE_GUID',
            'EFI_HARDWARE_ERROR_VARIABLE_GUID': 'EFI_HARDWARE_ERROR_VARIABLE',
            'EFI_GRAPHICS_INFO_HOB_GUID': 'EFI_PEI_GRAPHICS_INFO_HOB_GUID',
            'EFI_DEBUG_PORT_DEVICE_PATH_GUID': 'DEVICE_PATH_MESSAGING_DEBUGPORT',
            'EFI_RNG_ALGORITHM_SP80090_HASH256_GUID': 'EFI_RNG_ALGORITHM_SP800_90_HASH_256_GUID',
            'EFI_RNG_ALGORITHM_SP80090_HMAC256_GUID': 'EFI_RNG_ALGORITHM_SP800_90_HMAC_256_GUID',
            'EFI_RNG_ALGORITHM_SP80090_CTR256_GUID': 'EFI_RNG_ALGORITHM_SP800_90_CTR_256_GUID',
            'EFI_RNG_ALGORITHM_X9313_DES_GUID': 'EFI_RNG_ALGORITHM_X9_31_3DES_GUID',
            'EFI_RNG_ALGORITHM_X931_AES_GUID': 'EFI_RNG_ALGORITHM_X9_31_AES_GUID',
            'EFI_BLOCK_IO_CRYPTO_ALGO_AES_XTS_GUID': 'EFI_BLOCK_IO_CRYPTO_ALGO_GUID_AES_XTS',
            'EFI_BLOCK_IO_CRYPTO_ALGO_AES_CBC_MS_BITLOCKER_GUID': 'EFI_BLOCK_IO_CRYPTO_ALGO_GUID_AES_CBC_MICROSOFT_BITLOCKER',
            'EFI_HASH_ALGORITHM_MD5_GUID': 'EFI_HASH_ALGORTIHM_MD5_GUID',
            'EFI_HASH_ALGORITHM_SHA1_NO_PAD_GUID': 'EFI_HASH_ALGORITHM_SHA1_NOPAD_GUID',
            'EFI_HASH_ALGORITHM_SHA256_NO_PAD_GUID': 'EFI_HASH_ALGORITHM_SHA256_NOPAD_GUID',
            'EFI_SPI_CONFIGURATION_PROTOCOL_GUID': 'EFI_SPI_CONFIGURATION_GUID',
        }

        ignored_headers = {
            # These two headers conflict
            'Protocol/Rest.h',
            'Protocol/RestEx.h',

            'Protocol/UserManager.h',
            'Protocol/Kms.h',
            'Guid/Cper.h',
            'Guid/EventGroup.h',
        }

        # Go through all files in the include folder
        for root,dirs,files in os.walk('edk2/MdePkg/Include'):
            for file in files:
                with open(os.path.join(root, file)) as f:

                    guids = re.findall("\s*extern\s+EFI_GUID\s+g([a-zA-Z0-9_]+);", f.read())

                    if len(guids) > 0:

                        # Generate the include for the file
                        filename_include = os.path.join(root, file)[len('edk2/MdePkg/Include/'):]

                        if filename_include in ignored_headers:
                            continue

                        if filename_include.startswith('Pi/'):
                            continue

                        if filename_include.startswith('Ppi/'):
                            continue


                        if args.verbose == True:
                            print(filename_include)
                        guids_file.write('#include <{}>\n'.format(filename_include))

                        for guid_var_name in guids:

                            # Transform to snake case
                            guid_name = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', guid_var_name)
                            guid_name = re.sub('([a-z0-9])([A-Z])', r'\1_\2', guid_name).upper()

                            if guid_name in guid_name_fixes:
                                guid_name = guid_name_fixes[guid_name]

                            if args.verbose == True:
                                print(guid_name)
                            guids_file.write('EFI_GUID g{} = {};\n'.format(guid_var_name, guid_name))

                        guids_file.write('\n')
