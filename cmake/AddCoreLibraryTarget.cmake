
include(${SACN_CMAKE}/SacnSourceManifest.cmake)

function(sacn_add_core_library_target target_name)
  set (extra_args ${ARGN})
  list(LENGTH extra_args extra_count)
  if(${extra_count} GREATER 0)
    list(GET extra_args 0 config_dir)
  endif()
  if(${extra_count} GREATER 1)
    list(GET extra_args 1 ide_folder)
  endif()

  add_library(${target_name}
    ${SACN_PUBLIC_HEADERS}
    ${SACN_PRIVATE_HEADERS}
    ${SACN_SOURCES}
  )
  target_include_directories(${target_name}
    PUBLIC ${SACN_INCLUDE}
    PRIVATE ${SACN_SRC}
  )

  if(DEFINED config_dir)
    target_include_directories(${target_name} PUBLIC ${config_dir})
    target_compile_definitions(${target_name} PUBLIC SACN_HAVE_CONFIG_H)
  endif()

  if(SACN_SRTP_CRYPTO_POLICY STREQUAL "default")
    target_compile_definitions(${target_name} PUBLIC SACN_SRTP_CRYPTO_POLICY_DEFAULT=1)
  elseif(SACN_SRTP_CRYPTO_POLICY STREQUAL "aes-cm-128-hmac-sha1-32")
    target_compile_definitions(${target_name} PUBLIC SACN_SRTP_CRYPTO_POLICY_AES_CM_128_HMAC_SHA1_32=1)
  elseif(SACN_SRTP_CRYPTO_POLICY STREQUAL "aes-cm-128-null-auth")
    target_compile_definitions(${target_name} PUBLIC SACN_SRTP_CRYPTO_POLICY_AES_CM_128_NULL_AUTH=1)
  elseif(SACN_SRTP_CRYPTO_POLICY STREQUAL "null-cipher-hmac-sha1-80")
    target_compile_definitions(${target_name} PUBLIC SACN_SRTP_CRYPTO_POLICY_NULL_CIPHER_HMAC_SHA1_80=1)
  elseif(SACN_SRTP_CRYPTO_POLICY STREQUAL "null-cipher-hmac-null")
    target_compile_definitions(${target_name} PUBLIC SACN_SRTP_CRYPTO_POLICY_NULL_CIPHER_HMAC_NULL=1)
  elseif(SACN_SRTP_CRYPTO_POLICY STREQUAL "aes-cm-256-hmac-sha1-80")
    target_compile_definitions(${target_name} PUBLIC SACN_SRTP_CRYPTO_POLICY_AES_CM_256_HMAC_SHA1_80=1)
  elseif(SACN_SRTP_CRYPTO_POLICY STREQUAL "aes-cm-256-hmac-sha1-32")
    target_compile_definitions(${target_name} PUBLIC SACN_SRTP_CRYPTO_POLICY_AES_CM_256_HMAC_SHA1_32=1)
  elseif(SACN_SRTP_CRYPTO_POLICY STREQUAL "aes-cm-256-null-auth")
    target_compile_definitions(${target_name} PUBLIC SACN_SRTP_CRYPTO_POLICY_AES_CM_256_NULL_AUTH=1)
  elseif(SACN_SRTP_CRYPTO_POLICY STREQUAL "aes-cm-192-hmac-sha1-80")
    target_compile_definitions(${target_name} PUBLIC SACN_SRTP_CRYPTO_POLICY_AES_CM_192_HMAC_SHA1_80=1)
  elseif(SACN_SRTP_CRYPTO_POLICY STREQUAL "aes-cm-192-hmac-sha1-32")
    target_compile_definitions(${target_name} PUBLIC SACN_SRTP_CRYPTO_POLICY_AES_CM_192_HMAC_SHA1_32=1)
  elseif(SACN_SRTP_CRYPTO_POLICY STREQUAL "aes-cm-192-null-auth")
    target_compile_definitions(${target_name} PUBLIC SACN_SRTP_CRYPTO_POLICY_AES_CM_192_NULL_AUTH=1)
  elseif(SACN_SRTP_CRYPTO_POLICY STREQUAL "aes-gcm-128-16-auth")
    target_compile_definitions(${target_name} PUBLIC SACN_SRTP_CRYPTO_POLICY_AES_GCM_128_16_AUTH=1)
  elseif(SACN_SRTP_CRYPTO_POLICY STREQUAL "aes-gcm-256-16-auth")
    target_compile_definitions(${target_name} PUBLIC SACN_SRTP_CRYPTO_POLICY_AES_GCM_256_16_AUTH=1)
  endif()

  if(SACN_ENABLE_SRTP_REKEY_TEST)
    target_compile_definitions(${target_name} PUBLIC SACN_ENABLE_SRTP_REKEY_TEST=1)
  endif()

  if(SACN_ENABLE_SLOT_MIRRORING)
    target_compile_definitions(${target_name} PUBLIC SACN_ENABLE_SLOT_MIRRORING=1)
  endif()

  target_link_libraries(${target_name} PUBLIC EtcPal libSRTP::srtp3 mbedcrypto mbedx509 mbedtls)
  if(NOT COMPILING_AS_OSS)
    target_clang_tidy(${target_name})
  endif()

  # Organize sources in IDEs
  source_group(TREE ${SACN_SRC}/sacn PREFIX src FILES
    ${SACN_PRIVATE_HEADERS}
    ${SACN_SOURCES}
  )
  source_group(TREE ${SACN_INCLUDE}/sacn PREFIX include FILES ${SACN_PUBLIC_HEADERS})

  if(DEFINED ide_folder)
    set_target_properties(${target_name} PROPERTIES FOLDER ${ide_folder})
  endif()
endfunction()
