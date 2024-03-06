<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile doxygen_version="1.9.1" doxygen_gitid="ef9b20ac7f8a8621fcfc299f8bd0b80422390f4b">
  <compound kind="file">
    <name>common.h</name>
    <path>/tmp/tmpn_x69pdp/sacn/include/sacn/</path>
    <filename>common_8h.html</filename>
    <class kind="struct">SacnMcastInterface</class>
    <class kind="struct">SacnNetintConfig</class>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_NAME_MAX_LEN</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga0a68bef69f737e31072b475521d331c8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DMX_ADDRESS_COUNT</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga681f92a30c76ae426e2403a328978abb</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_REMOTE_SOURCE_INVALID</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga4579b31d1f4e1ecff0ecd8214c551956</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_STARTCODE_DMX</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga2dba81c3bf923ae2dbb0aaa6d8d5fa0a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_STARTCODE_PRIORITY</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga92ab22221d9dedb5b22978e6c14c6349</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>uint16_t</type>
      <name>sacn_remote_source_t</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga31b1febd91134668307803d573ed2f2c</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnMcastInterface</type>
      <name>SacnMcastInterface</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>gabf46c3c353abbd956716fecbe24f2ae4</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnNetintConfig</type>
      <name>SacnNetintConfig</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga3cd2196005e33f66518a0f0baba34147</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>sacn_ip_support_t</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga02f82b9c734e2d2f70a1106d6480833a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kSacnIpV4Only</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>gga02f82b9c734e2d2f70a1106d6480833aa900230541148a1eb50b457dfbf75a3c4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kSacnIpV6Only</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>gga02f82b9c734e2d2f70a1106d6480833aaa7c7cce56d48e45e33bde272cb0be424</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kSacnIpV4AndIpV6</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>gga02f82b9c734e2d2f70a1106d6480833aa946f61a87ca52ca76b687484c5cdced2</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_init</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga612160fe1d0f1e4f1fae4d72232fee07</anchor>
      <arglist>(const EtcPalLogParams *log_params, const SacnNetintConfig *sys_netint_config)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_deinit</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga7b80ebcafe9eb3240a67785377872f9a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>sacn_remote_source_t</type>
      <name>sacn_get_remote_source_handle</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga65d96208fc89676e2dea18d2ded31872</anchor>
      <arglist>(const EtcPalUuid *source_cid)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_get_remote_source_cid</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga9a71315342a299c22055fe195e6750ef</anchor>
      <arglist>(sacn_remote_source_t source_handle, EtcPalUuid *source_cid)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>common.h</name>
    <path>/tmp/tmpn_x69pdp/sacn/include/sacn/cpp/</path>
    <filename>cpp_2common_8h.html</filename>
    <includes id="common_8h" name="common.h" local="yes" imported="no">sacn/common.h</includes>
    <namespace>sacn</namespace>
    <member kind="typedef">
      <type>sacn_remote_source_t</type>
      <name>RemoteSourceHandle</name>
      <anchorfile>namespacesacn.html</anchorfile>
      <anchor>acfd8aea0d62baa7d2f16a969ec5849af</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>McastMode</name>
      <anchorfile>namespacesacn.html</anchorfile>
      <anchor>af874a6c4f11432c529c4068e745889b5</anchor>
      <arglist></arglist>
      <enumvalue file="namespacesacn.html" anchor="af874a6c4f11432c529c4068e745889b5ad1989e9a06422a85d3d6d2ecf25a50cf">kEnabledOnAllInterfaces</enumvalue>
      <enumvalue file="namespacesacn.html" anchor="af874a6c4f11432c529c4068e745889b5a42f053f48441c9286254c451c8bea6a2">kDisabledOnAllInterfaces</enumvalue>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Init</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>gac1cf1cb5e698e8ad656481cc834925db</anchor>
      <arglist>(const EtcPalLogParams *log_params=nullptr, McastMode mcast_mode=McastMode::kEnabledOnAllInterfaces)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Init</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>ga462a83ab46a4fa6a2f642fcacabecc82</anchor>
      <arglist>(const EtcPalLogParams *log_params, std::vector&lt; SacnMcastInterface &gt; &amp;sys_netints)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Init</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>ga73781de46ab1321166a0dce24094c73d</anchor>
      <arglist>(std::vector&lt; SacnMcastInterface &gt; &amp;sys_netints)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Init</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>ga0dfea4ef6503c65fd4e13eb9813e6513</anchor>
      <arglist>(const etcpal::Logger &amp;logger, McastMode mcast_mode=McastMode::kEnabledOnAllInterfaces)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Init</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>ga414aefdf3a0364b6261374e61a37bd05</anchor>
      <arglist>(const etcpal::Logger &amp;logger, std::vector&lt; SacnMcastInterface &gt; &amp;sys_netints)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Deinit</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>ga7c27553e8de8ffb78e3627f51fe9eb25</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>RemoteSourceHandle</type>
      <name>GetRemoteSourceHandle</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>gaa353377fefbbdd6ae634d981944c5619</anchor>
      <arglist>(const etcpal::Uuid &amp;source_cid)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Expected&lt; etcpal::Uuid &gt;</type>
      <name>GetRemoteSourceCid</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>gab05243a382f6332a3d73dbb3b202bb95</anchor>
      <arglist>(RemoteSourceHandle source_handle)</arglist>
    </member>
    <member kind="variable">
      <type>constexpr RemoteSourceHandle</type>
      <name>kInvalidRemoteSourceHandle</name>
      <anchorfile>namespacesacn.html</anchorfile>
      <anchor>a731047fe50bd58178f7cd8b1960e6e57</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>dmx_merger.h</name>
    <path>/tmp/tmpn_x69pdp/sacn/include/sacn/</path>
    <filename>dmx__merger_8h.html</filename>
    <includes id="common_8h" name="common.h" local="yes" imported="no">sacn/common.h</includes>
    <includes id="receiver_8h" name="receiver.h" local="yes" imported="no">sacn/receiver.h</includes>
    <class kind="struct">SacnDmxMergerConfig</class>
    <class kind="struct">SacnDmxMergerSource</class>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_INVALID</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga4578a59809c13ece174e8dcf59fce26d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_SOURCE_INVALID</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga1ca9e023e2091c5a8a4e5e2ba426f055</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_CONFIG_INIT</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga7431ab2baf3af7e0d1999355c6bcc9d7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_SOURCE_IS_VALID</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>gaec4962a6c5655b357d4c81dc9e7a7b86</anchor>
      <arglist>(owners_array, slot_index)</arglist>
    </member>
    <member kind="typedef">
      <type>int</type>
      <name>sacn_dmx_merger_t</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>gabee79fb378d5942866adc898cb7da38b</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>uint16_t</type>
      <name>sacn_dmx_merger_source_t</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga6c4761eedeaaf635ac495265849c07f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnDmxMergerConfig</type>
      <name>SacnDmxMergerConfig</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga9989e37ba8aa7aeb4ef0108ceb4e156c</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnDmxMergerSource</type>
      <name>SacnDmxMergerSource</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga5ff553cc00468871978edfe0e675aebd</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_dmx_merger_create</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga8758dad93531c1a5bbf4643157fe2c72</anchor>
      <arglist>(const SacnDmxMergerConfig *config, sacn_dmx_merger_t *handle)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_dmx_merger_destroy</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga539b249ea5d0898efce4ee7371fc91ef</anchor>
      <arglist>(sacn_dmx_merger_t handle)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_dmx_merger_add_source</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>gadf879eb673c0ffe91ade6bcc5af615c3</anchor>
      <arglist>(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t *source_id)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_dmx_merger_remove_source</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>gac6ebd9581fb8c6170d4acf1c24681ad7</anchor>
      <arglist>(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source)</arglist>
    </member>
    <member kind="function">
      <type>const SacnDmxMergerSource *</type>
      <name>sacn_dmx_merger_get_source</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga0811c88edb22c748c8b80bbc49984e67</anchor>
      <arglist>(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_dmx_merger_update_levels</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga6ee5e85689e75879fa1db01db0bad8a6</anchor>
      <arglist>(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source, const uint8_t *new_levels, size_t new_levels_count)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_dmx_merger_update_pap</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>gac28b72fd6849cb6815c0ca58c2d29e65</anchor>
      <arglist>(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source, const uint8_t *pap, size_t pap_count)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_dmx_merger_update_universe_priority</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>gab56b1f0e4fe8d5d1dd23695a1a861b3b</anchor>
      <arglist>(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source, uint8_t universe_priority)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_dmx_merger_remove_pap</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>gada109b5171e78bc858ab18a7c13931ee</anchor>
      <arglist>(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>dmx_merger.h</name>
    <path>/tmp/tmpn_x69pdp/sacn/include/sacn/cpp/</path>
    <filename>cpp_2dmx__merger_8h.html</filename>
    <includes id="cpp_2common_8h" name="common.h" local="yes" imported="no">sacn/cpp/common.h</includes>
    <includes id="dmx__merger_8h" name="dmx_merger.h" local="yes" imported="no">sacn/dmx_merger.h</includes>
    <class kind="class">sacn::detail::DmxMergerHandleType</class>
    <class kind="class">sacn::DmxMerger</class>
    <class kind="struct">sacn::DmxMerger::Settings</class>
    <namespace>sacn</namespace>
  </compound>
  <compound kind="file">
    <name>merge_receiver.h</name>
    <path>/tmp/tmpn_x69pdp/sacn/include/sacn/</path>
    <filename>merge__receiver_8h.html</filename>
    <includes id="receiver_8h" name="receiver.h" local="yes" imported="no">sacn/receiver.h</includes>
    <includes id="dmx__merger_8h" name="dmx_merger.h" local="yes" imported="no">sacn/dmx_merger.h</includes>
    <class kind="struct">SacnRecvMergedData</class>
    <class kind="struct">SacnMergeReceiverCallbacks</class>
    <class kind="struct">SacnMergeReceiverConfig</class>
    <class kind="struct">SacnMergeReceiverNetintList</class>
    <class kind="struct">SacnMergeReceiverSource</class>
    <member kind="define">
      <type>#define</type>
      <name>SACN_MERGE_RECEIVER_INVALID</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>gae86374c92bace3a7d2bef9656da6048a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_MERGE_RECEIVER_CONFIG_DEFAULT_INIT</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>gaeb89255ebd3592d97fe76ed6203913ae</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>int</type>
      <name>sacn_merge_receiver_t</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>gab674497f3bceb2d6ebf4b932b26bfe61</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnRecvMergedData</type>
      <name>SacnRecvMergedData</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>gafb05eeea1bbac03fd2058d197e2c5846</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnMergeReceiverMergedDataCallback</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga3b87b4a0324b476a926c86a18eab3a1d</anchor>
      <arglist>)(sacn_merge_receiver_t handle, const SacnRecvMergedData *merged_data, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnMergeReceiverNonDmxCallback</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga274346790ea25827de515b93f6df2c2b</anchor>
      <arglist>)(sacn_merge_receiver_t receiver_handle, const EtcPalSockAddr *source_addr, const SacnRemoteSource *source_info, const SacnRecvUniverseData *universe_data, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnMergeReceiverSourcesLostCallback</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga28f0d4119155beb794d94ef052ffe0c3</anchor>
      <arglist>)(sacn_merge_receiver_t handle, uint16_t universe, const SacnLostSource *lost_sources, size_t num_lost_sources, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnMergeReceiverSamplingPeriodStartedCallback</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga6bca38b03e63aa5a2e272449d23662ce</anchor>
      <arglist>)(sacn_merge_receiver_t handle, uint16_t universe, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnMergeReceiverSamplingPeriodEndedCallback</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga361d2c730abd2b490d99017f6ebb3739</anchor>
      <arglist>)(sacn_merge_receiver_t handle, uint16_t universe, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnMergeReceiverSourcePapLostCallback</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga3791403f2e07bd8df2364339f22d5b08</anchor>
      <arglist>)(sacn_merge_receiver_t handle, uint16_t universe, const SacnRemoteSource *source, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnMergeReceiverSourceLimitExceededCallback</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga47ccc08a4f5812e26ecf694fa0cb0ff0</anchor>
      <arglist>)(sacn_merge_receiver_t handle, uint16_t universe, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnMergeReceiverCallbacks</type>
      <name>SacnMergeReceiverCallbacks</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga92cb80327e80976e7b004bf9819bca0a</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnMergeReceiverConfig</type>
      <name>SacnMergeReceiverConfig</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga05a8525a8ee5280ab3b6e0583ae01ae8</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnMergeReceiverNetintList</type>
      <name>SacnMergeReceiverNetintList</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga2003a3a906229f10aaf77ad600ea1ad1</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnMergeReceiverSource</type>
      <name>SacnMergeReceiverSource</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>gac417e4563acfbf4bdfe8f2490c7e88c9</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_merge_receiver_config_init</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>gab0d6a624f1d44008625335112788eb37</anchor>
      <arglist>(SacnMergeReceiverConfig *config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_create</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga91ca26eea8b78eeb2b25bb003e8aa208</anchor>
      <arglist>(const SacnMergeReceiverConfig *config, sacn_merge_receiver_t *handle, const SacnNetintConfig *netint_config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_destroy</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga02c0879618435049e7a9bd25dbe58850</anchor>
      <arglist>(sacn_merge_receiver_t handle)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_get_universe</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga7345c390c8b94221b481c3cd1ca37de7</anchor>
      <arglist>(sacn_merge_receiver_t handle, uint16_t *universe_id)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_get_footprint</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga32dfa392c47b9f7a443e9bd563e1a974</anchor>
      <arglist>(sacn_merge_receiver_t handle, SacnRecvUniverseSubrange *footprint)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_change_universe</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga821122037b6cd927facd821c11cce970</anchor>
      <arglist>(sacn_merge_receiver_t handle, uint16_t new_universe_id)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_change_footprint</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga05a2a94b9c4346efa7918c7fe3031a80</anchor>
      <arglist>(sacn_merge_receiver_t handle, const SacnRecvUniverseSubrange *new_footprint)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_change_universe_and_footprint</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga99f34c7a019f2f4f5f62220a505c6e47</anchor>
      <arglist>(sacn_merge_receiver_t handle, uint16_t new_universe_id, const SacnRecvUniverseSubrange *new_footprint)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_reset_networking</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga00e28a2333ce8cd38b0eb3e58ac5f375</anchor>
      <arglist>(const SacnNetintConfig *sys_netint_config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_reset_networking_per_receiver</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga0db3ceb5a812f02e2e4daa52b5797523</anchor>
      <arglist>(const SacnNetintConfig *sys_netint_config, const SacnMergeReceiverNetintList *per_receiver_netint_lists, size_t num_per_receiver_netint_lists)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>sacn_merge_receiver_get_network_interfaces</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga01cecd351da958dc2fc7b55088559de8</anchor>
      <arglist>(sacn_merge_receiver_t handle, EtcPalMcastNetintId *netints, size_t netints_size)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_get_source</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga807515578955a36b0a33de53a1d5bd9c</anchor>
      <arglist>(sacn_merge_receiver_t merge_receiver_handle, sacn_remote_source_t source_handle, SacnMergeReceiverSource *source_info)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>merge_receiver.h</name>
    <path>/tmp/tmpn_x69pdp/sacn/include/sacn/cpp/</path>
    <filename>cpp_2merge__receiver_8h.html</filename>
    <includes id="cpp_2common_8h" name="common.h" local="yes" imported="no">sacn/cpp/common.h</includes>
    <includes id="merge__receiver_8h" name="merge_receiver.h" local="yes" imported="no">sacn/merge_receiver.h</includes>
    <class kind="class">sacn::detail::MergeReceiverHandleType</class>
    <class kind="class">sacn::MergeReceiver</class>
    <class kind="class">sacn::MergeReceiver::NotifyHandler</class>
    <class kind="struct">sacn::MergeReceiver::Settings</class>
    <class kind="struct">sacn::MergeReceiver::NetintList</class>
    <class kind="struct">sacn::MergeReceiver::Source</class>
    <namespace>sacn</namespace>
  </compound>
  <compound kind="file">
    <name>receiver.h</name>
    <path>/tmp/tmpn_x69pdp/sacn/include/sacn/</path>
    <filename>receiver_8h.html</filename>
    <includes id="common_8h" name="common.h" local="yes" imported="no">sacn/common.h</includes>
    <class kind="struct">SacnRecvUniverseSubrange</class>
    <class kind="struct">SacnRecvUniverseData</class>
    <class kind="struct">SacnRemoteSource</class>
    <class kind="struct">SacnLostSource</class>
    <class kind="struct">SacnReceiverCallbacks</class>
    <class kind="struct">SacnReceiverConfig</class>
    <class kind="struct">SacnReceiverNetintList</class>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_INVALID</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga79dd5d0d62fb4d6120290afeeadb3637</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_INFINITE_SOURCES</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gadb2ea19692692ca852423d0a9de749ea</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DEFAULT_EXPIRED_WAIT_MS</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga2f4617269e2d64c85b81556e0c3e8fde</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_CONFIG_DEFAULT_INIT</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gaaaa17f5e77d094f9348c0efd361cee52</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_OPTS_FILTER_PREVIEW_DATA</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga8613a5c435a6120a1d410bead3949087</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>int</type>
      <name>sacn_receiver_t</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gafc1e3c92911f567bed81bbd04f3f34f6</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnRecvUniverseSubrange</type>
      <name>SacnRecvUniverseSubrange</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gacd120c5410f4a19fe3faaeacbaaad904</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnRecvUniverseData</type>
      <name>SacnRecvUniverseData</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gac0c35d115d0d13af500b68eca8afda57</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnRemoteSource</type>
      <name>SacnRemoteSource</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga3fa6cd794c97a1c6c8330ec6b79aad38</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnLostSource</type>
      <name>SacnLostSource</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga7fdeb921a4c00c9821019145683bdda4</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnUniverseDataCallback</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga1ea1f87c1d1098d6df521cb9d9ccd0b3</anchor>
      <arglist>)(sacn_receiver_t receiver_handle, const EtcPalSockAddr *source_addr, const SacnRemoteSource *source_info, const SacnRecvUniverseData *universe_data, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnSourcesLostCallback</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga80326cc324898e1faebe2da1339bd0b3</anchor>
      <arglist>)(sacn_receiver_t handle, uint16_t universe, const SacnLostSource *lost_sources, size_t num_lost_sources, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnSamplingPeriodStartedCallback</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga5a47a2560a7aba67637d3a5b7ad22ef5</anchor>
      <arglist>)(sacn_receiver_t handle, uint16_t universe, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnSamplingPeriodEndedCallback</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gac9ac2e788bf69c6da712dfcd67269e67</anchor>
      <arglist>)(sacn_receiver_t handle, uint16_t universe, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnSourcePapLostCallback</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga7608a394e455465e56c789957f3f3214</anchor>
      <arglist>)(sacn_receiver_t handle, uint16_t universe, const SacnRemoteSource *source, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnSourceLimitExceededCallback</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga64bfe2ecfceca8b4f50f1a3a0a9c9a07</anchor>
      <arglist>)(sacn_receiver_t handle, uint16_t universe, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnReceiverCallbacks</type>
      <name>SacnReceiverCallbacks</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga114584027956aaaccfb9e74e1c311206</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnReceiverConfig</type>
      <name>SacnReceiverConfig</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gac54129e22da91ee8f1aaf9b56d771355</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnReceiverNetintList</type>
      <name>SacnReceiverNetintList</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga494aa6c2efd3b6a12943191c74946cb8</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_receiver_config_init</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga82f2e3740ce865b7aa3e018aa8a229d9</anchor>
      <arglist>(SacnReceiverConfig *config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_create</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gacc6321deb0647ac0175993be6083683b</anchor>
      <arglist>(const SacnReceiverConfig *config, sacn_receiver_t *handle, const SacnNetintConfig *netint_config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_destroy</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga43f99a2447364832c9ab135ac3d8b6ae</anchor>
      <arglist>(sacn_receiver_t handle)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_get_universe</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gae77eea445ebb380fb4d47c63c0f3ba32</anchor>
      <arglist>(sacn_receiver_t handle, uint16_t *universe_id)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_get_footprint</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga3dadba0fa169d0bfe82d5884004ecaa8</anchor>
      <arglist>(sacn_receiver_t handle, SacnRecvUniverseSubrange *footprint)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_change_universe</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gabb5ec7d4459ac694e5fa0ae97572f388</anchor>
      <arglist>(sacn_receiver_t handle, uint16_t new_universe_id)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_change_footprint</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gab5be9e77fa2652f71c93d9886ae826b2</anchor>
      <arglist>(sacn_receiver_t handle, const SacnRecvUniverseSubrange *new_footprint)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_change_universe_and_footprint</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gaf046362e62f8f4a910a00aaee3006247</anchor>
      <arglist>(sacn_receiver_t handle, uint16_t new_universe_id, const SacnRecvUniverseSubrange *new_footprint)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_reset_networking</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga54fd65585c71e83af35289c3cad0f685</anchor>
      <arglist>(const SacnNetintConfig *sys_netint_config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_reset_networking_per_receiver</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga29f330fc880d1c776afb5972514e6ac4</anchor>
      <arglist>(const SacnNetintConfig *sys_netint_config, const SacnReceiverNetintList *per_receiver_netint_lists, size_t num_per_receiver_netint_lists)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>sacn_receiver_get_network_interfaces</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga6ce5c1c8e4fafe2f339c6d3084dacfc4</anchor>
      <arglist>(sacn_receiver_t handle, EtcPalMcastNetintId *netints, size_t netints_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_receiver_set_expired_wait</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gabea27e71ae23c9176832f883a8ad7f06</anchor>
      <arglist>(uint32_t wait_ms)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>sacn_receiver_get_expired_wait</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gabd632f80d5da75c47c1f08103a42a391</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_OPTS_FILTER_PREVIEW_DATA</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga8613a5c435a6120a1d410bead3949087</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>receiver.h</name>
    <path>/tmp/tmpn_x69pdp/sacn/include/sacn/cpp/</path>
    <filename>cpp_2receiver_8h.html</filename>
    <includes id="cpp_2common_8h" name="common.h" local="yes" imported="no">sacn/cpp/common.h</includes>
    <includes id="receiver_8h" name="receiver.h" local="yes" imported="no">sacn/receiver.h</includes>
    <class kind="class">sacn::detail::ReceiverHandleType</class>
    <class kind="class">sacn::Receiver</class>
    <class kind="class">sacn::Receiver::NotifyHandler</class>
    <class kind="struct">sacn::Receiver::Settings</class>
    <class kind="struct">sacn::Receiver::NetintList</class>
    <namespace>sacn</namespace>
  </compound>
  <compound kind="file">
    <name>source.h</name>
    <path>/tmp/tmpn_x69pdp/sacn/include/sacn/</path>
    <filename>source_8h.html</filename>
    <includes id="common_8h" name="common.h" local="yes" imported="no">sacn/common.h</includes>
    <class kind="struct">SacnSourceConfig</class>
    <class kind="struct">SacnSourceUniverseConfig</class>
    <class kind="struct">SacnSourceUniverseNetintList</class>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_INVALID</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga446e9e065e08c7a90309a993b2502153</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_INFINITE_UNIVERSES</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga1ac5056bc752c32ca80c08b9839a142d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_KEEP_ALIVE_INTERVAL_DEFAULT</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gab3c248f42fcdeccba28617fd2612ce71</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_PAP_KEEP_ALIVE_INTERVAL_DEFAULT</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gae3df139429df836f977bab01da3af19d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_CONFIG_DEFAULT_INIT</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga1fc88ea9a51c4a935ec87630a0b177f1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga20483046bfb90da7c9bf089fc0229a6b</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>int</type>
      <name>sacn_source_t</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gacc4c9d2c77cf4126e9ac7faf297c3dd8</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnSourceConfig</type>
      <name>SacnSourceConfig</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gaf13f724dda8304930fcda0977088cd6a</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnSourceUniverseConfig</type>
      <name>SacnSourceUniverseConfig</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga3f72b89ed47da0e268c7735ae8971e89</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnSourceUniverseNetintList</type>
      <name>SacnSourceUniverseNetintList</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga0f30b2db7d3c6686a14bab97c80d73a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>sacn_source_tick_mode_t</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gaa32b392ec2a472f4b42d1395fa1d047d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kSacnSourceTickModeProcessLevelsOnly</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ggaa32b392ec2a472f4b42d1395fa1d047dad2e8539866e4da26bfaaad3f7fea354e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kSacnSourceTickModeProcessPapOnly</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ggaa32b392ec2a472f4b42d1395fa1d047daa135ff244f5bde7171ce9f53cb353280</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kSacnSourceTickModeProcessLevelsAndPap</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ggaa32b392ec2a472f4b42d1395fa1d047da811ce8e71a44eccfcf82135395792e61</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_config_init</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga4d115a7b828b0f45444ac36f0b946e64</anchor>
      <arglist>(SacnSourceConfig *config)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_universe_config_init</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga79d292e6589859e52e82834c611311ea</anchor>
      <arglist>(SacnSourceUniverseConfig *config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_create</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga37f43cd2ec265e3052eea6d5f1b68233</anchor>
      <arglist>(const SacnSourceConfig *config, sacn_source_t *handle)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_destroy</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gaeb8a9797368aabad696c81225b4c8aaa</anchor>
      <arglist>(sacn_source_t handle)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_change_name</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga0c44cd3dba2627779bfb65ea8349fe1c</anchor>
      <arglist>(sacn_source_t handle, const char *new_name)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_add_universe</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga22a80913e48354c6da37a9fa11626261</anchor>
      <arglist>(sacn_source_t handle, const SacnSourceUniverseConfig *config, const SacnNetintConfig *netint_config)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_remove_universe</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga7f8233fae115142c2e2a9488faa12d5d</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>sacn_source_get_universes</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga90b2e59fa6709ee72042778a60c93330</anchor>
      <arglist>(sacn_source_t handle, uint16_t *universes, size_t universes_size)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_add_unicast_destination</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga2b7d3fb6a2d252eee7c051acfe97d242</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr *dest)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_remove_unicast_destination</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga4ebe5d509af8c963c104f2dd96a3ac76</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr *dest)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>sacn_source_get_unicast_destinations</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga12f792b85add6af90ba2560f287b630c</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, EtcPalIpAddr *destinations, size_t destinations_size)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_change_priority</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga0ee9dd3d8114b4277cb73134d0ae2dc4</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, uint8_t new_priority)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_change_preview_flag</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga99d5fce087ce0b2b7d2caedf65bcce5e</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, bool new_preview_flag)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_change_synchronization_universe</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gac21fab7b1fe15c281e89303ce26d08fd</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, uint16_t new_sync_universe)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_send_now</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga6e7cea16d61ae355bc313452f0b47e48</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, uint8_t start_code, const uint8_t *buffer, size_t buflen)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_send_synchronization</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga0e07a954dab245c7d6f323d9bdeb2df3</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_update_levels</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga23dc503a9f5a46fce7e8448266760341</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, const uint8_t *new_levels, size_t new_levels_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_update_levels_and_pap</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga6a8835b680485d307f9d2d9c750517ab</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, const uint8_t *new_levels, size_t new_levels_size, const uint8_t *new_priorities, size_t new_priorities_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_update_levels_and_force_sync</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga56ea27101d95616ca53c5c602257311a</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, const uint8_t *new_levels, size_t new_levels_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_update_levels_and_pap_and_force_sync</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gabec53e52dbb1808a546832ef3446f009</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, const uint8_t *new_levels, size_t new_levels_size, const uint8_t *new_priorities, size_t new_priorities_size)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>sacn_source_process_manual</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga9d9b3e992e56375779651023cc92121d</anchor>
      <arglist>(sacn_source_tick_mode_t tick_mode)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_reset_networking</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga348c4a38439a1bf2623253bbc30120c5</anchor>
      <arglist>(const SacnNetintConfig *sys_netint_config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_reset_networking_per_universe</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gac47eb25e0017cf7d62120fb3fa072333</anchor>
      <arglist>(const SacnNetintConfig *sys_netint_config, const SacnSourceUniverseNetintList *per_universe_netint_lists, size_t num_per_universe_netint_lists)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>sacn_source_get_network_interfaces</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gae8c8d77375ea48b26fc069c9068aec69</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, EtcPalMcastNetintId *netints, size_t netints_size)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>source.h</name>
    <path>/tmp/tmpn_x69pdp/sacn/include/sacn/cpp/</path>
    <filename>cpp_2source_8h.html</filename>
    <includes id="cpp_2common_8h" name="common.h" local="yes" imported="no">sacn/cpp/common.h</includes>
    <includes id="source_8h" name="source.h" local="yes" imported="no">sacn/source.h</includes>
    <class kind="class">sacn::detail::SourceHandleType</class>
    <class kind="class">sacn::Source</class>
    <class kind="struct">sacn::Source::Settings</class>
    <class kind="struct">sacn::Source::UniverseSettings</class>
    <class kind="struct">sacn::Source::UniverseNetintList</class>
    <namespace>sacn</namespace>
  </compound>
  <compound kind="file">
    <name>source_detector.h</name>
    <path>/tmp/tmpn_x69pdp/sacn/include/sacn/</path>
    <filename>source__detector_8h.html</filename>
    <includes id="common_8h" name="common.h" local="yes" imported="no">sacn/common.h</includes>
    <class kind="struct">SacnSourceDetectorCallbacks</class>
    <class kind="struct">SacnSourceDetectorConfig</class>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_DETECTOR_INFINITE</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>ga2a1bee1e4fe7a47ee053c8dbae05fbcb</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_DETECTOR_CONFIG_DEFAULT_INIT</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>ga78fbb7639c835e3b08091fc16e31d6fc</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnSourceDetectorSourceUpdatedCallback</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>ga30a59185e89668385ea425238cab8422</anchor>
      <arglist>)(sacn_remote_source_t handle, const EtcPalUuid *cid, const char *name, const uint16_t *sourced_universes, size_t num_sourced_universes, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnSourceDetectorSourceExpiredCallback</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>ga18cf47c111cd4d23ee36c8aae4c97158</anchor>
      <arglist>)(sacn_remote_source_t handle, const EtcPalUuid *cid, const char *name, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnSourceDetectorLimitExceededCallback</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>gadb2603e053f842705b654047e99c0096</anchor>
      <arglist>)(void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnSourceDetectorCallbacks</type>
      <name>SacnSourceDetectorCallbacks</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>gaa539e91ba47bdb365998472761dcede2</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnSourceDetectorConfig</type>
      <name>SacnSourceDetectorConfig</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>gacf1948692a93767d849b6df90f75f6a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_detector_config_init</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>gaaf2f5a453a68df1ed4dcc80e1a94a49a</anchor>
      <arglist>(SacnSourceDetectorConfig *config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_detector_create</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>gaa662eb1cc3bb937691dcabc56332952f</anchor>
      <arglist>(const SacnSourceDetectorConfig *config, const SacnNetintConfig *netint_config)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_detector_destroy</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>ga42bf1f29fa1a313343b672e33685ba1d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_detector_reset_networking</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>ga740a10eb54b67decf41794e298258a21</anchor>
      <arglist>(const SacnNetintConfig *sys_netint_config)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>sacn_source_detector_get_network_interfaces</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>gae0c2cafc1d5d765f272981fc06f5fdc6</anchor>
      <arglist>(EtcPalMcastNetintId *netints, size_t netints_size)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>source_detector.h</name>
    <path>/tmp/tmpn_x69pdp/sacn/include/sacn/cpp/</path>
    <filename>cpp_2source__detector_8h.html</filename>
    <includes id="cpp_2common_8h" name="common.h" local="yes" imported="no">sacn/cpp/common.h</includes>
    <includes id="source__detector_8h" name="source_detector.h" local="yes" imported="no">sacn/source_detector.h</includes>
    <class kind="class">sacn::SourceDetector</class>
    <class kind="class">sacn::SourceDetector::NotifyHandler</class>
    <class kind="struct">sacn::SourceDetector::Settings</class>
    <namespace>sacn</namespace>
  </compound>
  <compound kind="file">
    <name>version.h</name>
    <path>/tmp/tmpn_x69pdp/sacn/include/sacn/</path>
    <filename>version_8h.html</filename>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_MAJOR</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga3b04b863e88d1bae02133dfb19667e06</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_MINOR</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga234bcbd2198002c6ee8d3caa670ba0ed</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_PATCH</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>gae1d8849ebfa2d27cec433e54f7f7308d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_BUILD</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>gab22648c510945c218b806ad28e1e9a86</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_STRING</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga2ff6980847182dc1ac56ee3660e0a360</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_DATESTR</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga99a95e107267c6f80cc2195f86c11586</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_COPYRIGHT</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga5fd2e6c86426807d2eb598c67121723b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_PRODUCTNAME</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga46b3c38236732e08243f8fe79bcf6c06</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_MAJOR</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga3b04b863e88d1bae02133dfb19667e06</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_MINOR</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga234bcbd2198002c6ee8d3caa670ba0ed</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_PATCH</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>gae1d8849ebfa2d27cec433e54f7f7308d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_BUILD</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>gab22648c510945c218b806ad28e1e9a86</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_STRING</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga2ff6980847182dc1ac56ee3660e0a360</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_DATESTR</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga99a95e107267c6f80cc2195f86c11586</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_COPYRIGHT</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga5fd2e6c86426807d2eb598c67121723b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_PRODUCTNAME</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga46b3c38236732e08243f8fe79bcf6c06</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>opts.h</name>
    <path>/tmp/tmpn_x69pdp/sacn/src/sacn/private/</path>
    <filename>opts_8h.html</filename>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DYNAMIC_MEM</name>
      <anchorfile>group__sacnopts__global.html</anchorfile>
      <anchor>gaf7b1d2fa482d1665683883f80b1f4d87</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_LOGGING_ENABLED</name>
      <anchorfile>group__sacnopts__global.html</anchorfile>
      <anchor>ga5b77f40b283fa0f754576a64ff6f3d4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_LOG_MSG_PREFIX</name>
      <anchorfile>group__sacnopts__global.html</anchorfile>
      <anchor>ga739093ac67975234540c307629ac8280</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_ASSERT_VERIFY</name>
      <anchorfile>group__sacnopts__global.html</anchorfile>
      <anchor>ga5a66b273b21d3c30e4531a590a480d2c</anchor>
      <arglist>(exp)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_ASSERT</name>
      <anchorfile>group__sacnopts__global.html</anchorfile>
      <anchor>gabd4f61db43336221d5947896ef0f2921</anchor>
      <arglist>(expr)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_ETC_PRIORITY_EXTENSION</name>
      <anchorfile>group__sacnopts__global.html</anchorfile>
      <anchor>ga59ee3abb2f4eb1554d7be219a9c8028b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_LOOPBACK</name>
      <anchorfile>group__sacnopts__global.html</anchorfile>
      <anchor>ga6ae6b1e22f87a5c2e49aefbc95a97256</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_MAX_NETINTS</name>
      <anchorfile>group__sacnopts__global.html</anchorfile>
      <anchor>ga2dd2edc6a9d13618baaf6d02b4d86e81</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_THREAD_PRIORITY</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga0262024b708fa546807d7b01485c7fcf</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_THREAD_STACK</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga3194913a252cb1da68e5bd919f6c1658</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_THREAD_NAME</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga0b3b952f010e515ba69fed3767e905f1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_READ_TIMEOUT_MS</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>gae7ae8c07912489f40fa77146cdc93d71</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_MAX_UNIVERSES</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga5d749b5a5b67d89a114aa2409ba6ff62</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga079b2f51919b9ac76e3b3330040bd20e</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_TOTAL_MAX_SOURCES</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga055b24b2c72073823f4c92ed32e7ddb0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_LIMIT_BIND</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga6bfc294d941da830fbb9bd562c45f638</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_SOCKET_PER_NIC</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>gac9daa024fb951f98cdf9c97ddc8244dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_MAX_SUBS_PER_SOCKET</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>gab20e9727556f9656a506bb85628f7df8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_ENABLE_SO_REUSEPORT</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>gaeed18ca4945f91d2108bdc32e587cf18</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_ENABLE_SO_RCVBUF</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga45b589b1c57b1812592ae77df67bd754</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_MAX_THREADS</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga064a427072df41e909b7d78cdb233c64</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_MAX_FOOTPRINT</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>gaf0e0364237fe1e26c348fdd53bb5976f</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_THREAD_PRIORITY</name>
      <anchorfile>group__sacnopts__send.html</anchorfile>
      <anchor>gac69e2ffb7e6a156134373d687a7cbed7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_THREAD_STACK</name>
      <anchorfile>group__sacnopts__send.html</anchorfile>
      <anchor>ga197a7ace1de904752a545c799ea9e498</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_THREAD_NAME</name>
      <anchorfile>group__sacnopts__send.html</anchorfile>
      <anchor>ga0a2bddcd541aabbd0b4536d6dae4caa2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_SOCKET_SNDBUF_SIZE</name>
      <anchorfile>group__sacnopts__send.html</anchorfile>
      <anchor>gafb37b9b4569cb297e919655ad1f0ff22</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_MAX_SOURCES</name>
      <anchorfile>group__sacnopts__send.html</anchorfile>
      <anchor>gaa08afe863ab6922c173055d4239184c4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE</name>
      <anchorfile>group__sacnopts__send.html</anchorfile>
      <anchor>gaac75390f2990b299303bfbce89e5f7ef</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_MAX_UNICAST_DESTINATIONS_PER_UNIVERSE</name>
      <anchorfile>group__sacnopts__send.html</anchorfile>
      <anchor>ga53a3e7e498368c3311403912d7c01515</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_MAX_MERGERS</name>
      <anchorfile>group__sacnopts__dmx__merger.html</anchorfile>
      <anchor>ga7d15f54f43af8434b4967746f1282fb1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER</name>
      <anchorfile>group__sacnopts__dmx__merger.html</anchorfile>
      <anchor>gaaeaa9701ec2b290ddfb3b31d8463dca4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_MAX_SLOTS</name>
      <anchorfile>group__sacnopts__dmx__merger.html</anchorfile>
      <anchor>ga01ed1a377484d83e568bd7cf053f392f</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_DISABLE_INTERNAL_PAP_BUFFER</name>
      <anchorfile>group__sacnopts__dmx__merger.html</anchorfile>
      <anchor>ga1291670a95116043ee7489006427aa5c</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_DISABLE_INTERNAL_OWNER_BUFFER</name>
      <anchorfile>group__sacnopts__dmx__merger.html</anchorfile>
      <anchor>ga79ce98d0c9b30964fd17728dc8073a26</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_MERGE_RECEIVER_ENABLE_IN_STATIC_MEMORY_MODE</name>
      <anchorfile>group__sacnopts__merge__receiver.html</anchorfile>
      <anchor>gaedd4882cf4778846d5e3e244fea9c916</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_MERGE_RECEIVER_ENABLE_SAMPLING_MERGER</name>
      <anchorfile>group__sacnopts__merge__receiver.html</anchorfile>
      <anchor>ga2e082ccf74be5a3b106b8623f5eaa65d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_MAX_MERGERS</name>
      <anchorfile>group__sacnopts__dmx__merger.html</anchorfile>
      <anchor>ga7d15f54f43af8434b4967746f1282fb1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_DETECTOR_MAX_SOURCES</name>
      <anchorfile>group__sacnopts__source__detector.html</anchorfile>
      <anchor>ga872fc4bd419f3cc3d77e7502f0b1ed0e</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE</name>
      <anchorfile>group__sacnopts__source__detector.html</anchorfile>
      <anchor>ga24e186592053bd29fbffbfc92825268e</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>sacn::DmxMerger</name>
    <filename>classsacn_1_1_dmx_merger.html</filename>
    <class kind="struct">sacn::DmxMerger::Settings</class>
    <member kind="typedef">
      <type>etcpal::OpaqueId&lt; detail::DmxMergerHandleType, sacn_dmx_merger_t, SACN_DMX_MERGER_INVALID &gt;</type>
      <name>Handle</name>
      <anchorfile>classsacn_1_1_dmx_merger.html</anchorfile>
      <anchor>a7b4c9f6ffcd98ecf441928151d890453</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DmxMerger</name>
      <anchorfile>classsacn_1_1_dmx_merger.html</anchorfile>
      <anchor>a18d71ec061f6533c49251873b493818f</anchor>
      <arglist>(DmxMerger &amp;&amp;other)=default</arglist>
    </member>
    <member kind="function">
      <type>DmxMerger &amp;</type>
      <name>operator=</name>
      <anchorfile>classsacn_1_1_dmx_merger.html</anchorfile>
      <anchor>a95bbd64563cbe066ee91cc07cf85db9b</anchor>
      <arglist>(DmxMerger &amp;&amp;other)=default</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Startup</name>
      <anchorfile>classsacn_1_1_dmx_merger.html</anchorfile>
      <anchor>ab263565edc9ebf5ffd6e82a18184e4c3</anchor>
      <arglist>(const Settings &amp;settings)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Shutdown</name>
      <anchorfile>classsacn_1_1_dmx_merger.html</anchorfile>
      <anchor>ac5f038c2b480cf9ef5e19e3eba8dbaf9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Expected&lt; sacn_dmx_merger_source_t &gt;</type>
      <name>AddSource</name>
      <anchorfile>classsacn_1_1_dmx_merger.html</anchorfile>
      <anchor>ac4c679c71b50cde6301145a91b8440ce</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>RemoveSource</name>
      <anchorfile>classsacn_1_1_dmx_merger.html</anchorfile>
      <anchor>a826044729a5c70ee16d5fbc26f93414b</anchor>
      <arglist>(sacn_dmx_merger_source_t source)</arglist>
    </member>
    <member kind="function">
      <type>const SacnDmxMergerSource *</type>
      <name>GetSourceInfo</name>
      <anchorfile>classsacn_1_1_dmx_merger.html</anchorfile>
      <anchor>a5f5b58945088002ac617cc5b80cd6fa2</anchor>
      <arglist>(sacn_dmx_merger_source_t source) const</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>UpdateLevels</name>
      <anchorfile>classsacn_1_1_dmx_merger.html</anchorfile>
      <anchor>a1aa19e03ca654b84ed0574bb4c4b5659</anchor>
      <arglist>(sacn_dmx_merger_source_t source, const uint8_t *new_levels, size_t new_levels_count)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>UpdatePap</name>
      <anchorfile>classsacn_1_1_dmx_merger.html</anchorfile>
      <anchor>a6954a0af298bdb2c17829a129ce0aad6</anchor>
      <arglist>(sacn_dmx_merger_source_t source, const uint8_t *pap, size_t pap_count)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>UpdateUniversePriority</name>
      <anchorfile>classsacn_1_1_dmx_merger.html</anchorfile>
      <anchor>ada8d157cee674178ec576caa111afca9</anchor>
      <arglist>(sacn_dmx_merger_source_t source, uint8_t universe_priority)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>RemovePap</name>
      <anchorfile>classsacn_1_1_dmx_merger.html</anchorfile>
      <anchor>ae33a69fe3b51efd79d384a462efcd64a</anchor>
      <arglist>(sacn_dmx_merger_source_t source)</arglist>
    </member>
    <member kind="function">
      <type>constexpr Handle</type>
      <name>handle</name>
      <anchorfile>classsacn_1_1_dmx_merger.html</anchorfile>
      <anchor>a7a9405a7d27a0e79c0cc00f8a7e2e2e7</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>sacn::detail::DmxMergerHandleType</name>
    <filename>classsacn_1_1detail_1_1_dmx_merger_handle_type.html</filename>
  </compound>
  <compound kind="class">
    <name>sacn::MergeReceiver</name>
    <filename>classsacn_1_1_merge_receiver.html</filename>
    <class kind="struct">sacn::MergeReceiver::NetintList</class>
    <class kind="class">sacn::MergeReceiver::NotifyHandler</class>
    <class kind="struct">sacn::MergeReceiver::Settings</class>
    <class kind="struct">sacn::MergeReceiver::Source</class>
    <member kind="typedef">
      <type>etcpal::OpaqueId&lt; detail::MergeReceiverHandleType, sacn_merge_receiver_t, SACN_MERGE_RECEIVER_INVALID &gt;</type>
      <name>Handle</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>a4a5bdec680598648c6dbfae0422ef922</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MergeReceiver</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>acd894017c62b549662194bd84a9f4a15</anchor>
      <arglist>(MergeReceiver &amp;&amp;other)=default</arglist>
    </member>
    <member kind="function">
      <type>MergeReceiver &amp;</type>
      <name>operator=</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>a7778785bf8c7ee8aaf1708c6c188cced</anchor>
      <arglist>(MergeReceiver &amp;&amp;other)=default</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Startup</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>a1fa0e47dc34289f2cffabdfc69c216a2</anchor>
      <arglist>(const Settings &amp;settings, NotifyHandler &amp;notify_handler, McastMode mcast_mode)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Startup</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>abb65535d2f1ed30a79014d81eb7f6807</anchor>
      <arglist>(const Settings &amp;settings, NotifyHandler &amp;notify_handler, std::vector&lt; SacnMcastInterface &gt; &amp;netints)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Shutdown</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>ac5f038c2b480cf9ef5e19e3eba8dbaf9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Expected&lt; uint16_t &gt;</type>
      <name>GetUniverse</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>a3d205f22d3d18f81585fd8d07ea21647</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Expected&lt; SacnRecvUniverseSubrange &gt;</type>
      <name>GetFootprint</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>aaa4d2448f1e1f8b23c095742738df97c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>ChangeUniverse</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>aa94ae64c11fbb22db3e21fba1e85cc64</anchor>
      <arglist>(uint16_t new_universe_id)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>ChangeFootprint</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>a6d46635ac84bcb47ff0d01bcafe4ef91</anchor>
      <arglist>(const SacnRecvUniverseSubrange &amp;new_footprint)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>ChangeUniverseAndFootprint</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>ac097835505ac503b525bcaa566cf506d</anchor>
      <arglist>(uint16_t new_universe_id, const SacnRecvUniverseSubrange &amp;new_footprint)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; EtcPalMcastNetintId &gt;</type>
      <name>GetNetworkInterfaces</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>a227c8165a2af9b39ac6d53cbea18d121</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Expected&lt; Source &gt;</type>
      <name>GetSource</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>aae0754f40a65bc30573808504455cfe5</anchor>
      <arglist>(sacn_remote_source_t source_handle)</arglist>
    </member>
    <member kind="function">
      <type>constexpr Handle</type>
      <name>handle</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>aa4a2884ccfef0338bcb1985f86ae89b7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static etcpal::Error</type>
      <name>ResetNetworking</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>aab11585b71f32bd1973f307435ffcd5b</anchor>
      <arglist>(McastMode mcast_mode)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static etcpal::Error</type>
      <name>ResetNetworking</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>a26dd2b3b95dbaac498bfc0ba231f42e6</anchor>
      <arglist>(std::vector&lt; SacnMcastInterface &gt; &amp;netints)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static etcpal::Error</type>
      <name>ResetNetworking</name>
      <anchorfile>classsacn_1_1_merge_receiver.html</anchorfile>
      <anchor>a8c0764f462388260aa5a6ab3fec2359c</anchor>
      <arglist>(std::vector&lt; SacnMcastInterface &gt; &amp;sys_netints, std::vector&lt; NetintList &gt; &amp;netint_lists)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>sacn::detail::MergeReceiverHandleType</name>
    <filename>classsacn_1_1detail_1_1_merge_receiver_handle_type.html</filename>
  </compound>
  <compound kind="struct">
    <name>sacn::MergeReceiver::NetintList</name>
    <filename>structsacn_1_1_merge_receiver_1_1_netint_list.html</filename>
    <member kind="function">
      <type></type>
      <name>NetintList</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_netint_list.html</anchorfile>
      <anchor>aab5d1e8b9c2879521f59b7a50388e1a3</anchor>
      <arglist>()=default</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>NetintList</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_netint_list.html</anchorfile>
      <anchor>a8d90a29c2e9f8926b9b3ff64458392d2</anchor>
      <arglist>(sacn_merge_receiver_t merge_receiver_handle, McastMode mcast_mode)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>NetintList</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_netint_list.html</anchorfile>
      <anchor>a7f4b990c2355dccdf25025a56358b6fd</anchor>
      <arglist>(sacn_merge_receiver_t merge_receiver_handle, const std::vector&lt; SacnMcastInterface &gt; &amp;network_interfaces)</arglist>
    </member>
    <member kind="variable">
      <type>sacn_merge_receiver_t</type>
      <name>handle</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_netint_list.html</anchorfile>
      <anchor>aba7e88ef8eda8612d97729978383ba05</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>std::vector&lt; SacnMcastInterface &gt;</type>
      <name>netints</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_netint_list.html</anchorfile>
      <anchor>a2a0cd7263bacb4663085d28599867b12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>no_netints</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_netint_list.html</anchorfile>
      <anchor>a45fc59834ac15fd6d3c71454af1d247a</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>sacn::Receiver::NetintList</name>
    <filename>structsacn_1_1_receiver_1_1_netint_list.html</filename>
    <member kind="function">
      <type></type>
      <name>NetintList</name>
      <anchorfile>structsacn_1_1_receiver_1_1_netint_list.html</anchorfile>
      <anchor>aab5d1e8b9c2879521f59b7a50388e1a3</anchor>
      <arglist>()=default</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>NetintList</name>
      <anchorfile>structsacn_1_1_receiver_1_1_netint_list.html</anchorfile>
      <anchor>ac0e52c9547dd642b53ff3f32d79856ec</anchor>
      <arglist>(sacn_receiver_t receiver_handle, McastMode mcast_mode)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>NetintList</name>
      <anchorfile>structsacn_1_1_receiver_1_1_netint_list.html</anchorfile>
      <anchor>a055503469cbefb1d3f658e9c8b55a5ba</anchor>
      <arglist>(sacn_receiver_t receiver_handle, const std::vector&lt; SacnMcastInterface &gt; &amp;network_interfaces)</arglist>
    </member>
    <member kind="variable">
      <type>sacn_receiver_t</type>
      <name>handle</name>
      <anchorfile>structsacn_1_1_receiver_1_1_netint_list.html</anchorfile>
      <anchor>a0d237fd6d38af257c8ab2a14a64a76e0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>std::vector&lt; SacnMcastInterface &gt;</type>
      <name>netints</name>
      <anchorfile>structsacn_1_1_receiver_1_1_netint_list.html</anchorfile>
      <anchor>a2a0cd7263bacb4663085d28599867b12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>no_netints</name>
      <anchorfile>structsacn_1_1_receiver_1_1_netint_list.html</anchorfile>
      <anchor>a45fc59834ac15fd6d3c71454af1d247a</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>sacn::MergeReceiver::NotifyHandler</name>
    <filename>classsacn_1_1_merge_receiver_1_1_notify_handler.html</filename>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>HandleMergedData</name>
      <anchorfile>classsacn_1_1_merge_receiver_1_1_notify_handler.html</anchorfile>
      <anchor>a0d46e56c609d6edd9806994bb81190fc</anchor>
      <arglist>(Handle handle, const SacnRecvMergedData &amp;merged_data)=0</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>HandleNonDmxData</name>
      <anchorfile>classsacn_1_1_merge_receiver_1_1_notify_handler.html</anchorfile>
      <anchor>a7d7240a815773ac4a959a56d553588b7</anchor>
      <arglist>(Handle receiver_handle, const etcpal::SockAddr &amp;source_addr, const SacnRemoteSource &amp;source_info, const SacnRecvUniverseData &amp;universe_data)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>HandleSourcesLost</name>
      <anchorfile>classsacn_1_1_merge_receiver_1_1_notify_handler.html</anchorfile>
      <anchor>ae2d27b76147fba2f4d4ed5219d614246</anchor>
      <arglist>(Handle handle, uint16_t universe, const std::vector&lt; SacnLostSource &gt; &amp;lost_sources)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>HandleSamplingPeriodStarted</name>
      <anchorfile>classsacn_1_1_merge_receiver_1_1_notify_handler.html</anchorfile>
      <anchor>a9cd14450c1a43ab482bebaf5a1dcb0cc</anchor>
      <arglist>(Handle handle, uint16_t universe)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>HandleSamplingPeriodEnded</name>
      <anchorfile>classsacn_1_1_merge_receiver_1_1_notify_handler.html</anchorfile>
      <anchor>af02397cc6bf09294666b38883dbdf6a5</anchor>
      <arglist>(Handle handle, uint16_t universe)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>HandleSourcePapLost</name>
      <anchorfile>classsacn_1_1_merge_receiver_1_1_notify_handler.html</anchorfile>
      <anchor>afc6fd518860704ac4b1f58d5a24f6672</anchor>
      <arglist>(Handle handle, uint16_t universe, const SacnRemoteSource &amp;source)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>HandleSourceLimitExceeded</name>
      <anchorfile>classsacn_1_1_merge_receiver_1_1_notify_handler.html</anchorfile>
      <anchor>ab517496e89eb88b7ee7fa383558f725b</anchor>
      <arglist>(Handle handle, uint16_t universe)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>sacn::Receiver::NotifyHandler</name>
    <filename>classsacn_1_1_receiver_1_1_notify_handler.html</filename>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>HandleUniverseData</name>
      <anchorfile>classsacn_1_1_receiver_1_1_notify_handler.html</anchorfile>
      <anchor>a31ed01381fa689d55d77f298fdf3c2c3</anchor>
      <arglist>(Handle receiver_handle, const etcpal::SockAddr &amp;source_addr, const SacnRemoteSource &amp;source_info, const SacnRecvUniverseData &amp;universe_data)=0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>HandleSourcesLost</name>
      <anchorfile>classsacn_1_1_receiver_1_1_notify_handler.html</anchorfile>
      <anchor>aeb67059ed834bf3d178bbdc4a931adae</anchor>
      <arglist>(Handle handle, uint16_t universe, const std::vector&lt; SacnLostSource &gt; &amp;lost_sources)=0</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>HandleSamplingPeriodStarted</name>
      <anchorfile>classsacn_1_1_receiver_1_1_notify_handler.html</anchorfile>
      <anchor>a9cd14450c1a43ab482bebaf5a1dcb0cc</anchor>
      <arglist>(Handle handle, uint16_t universe)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>HandleSamplingPeriodEnded</name>
      <anchorfile>classsacn_1_1_receiver_1_1_notify_handler.html</anchorfile>
      <anchor>af02397cc6bf09294666b38883dbdf6a5</anchor>
      <arglist>(Handle handle, uint16_t universe)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>HandleSourcePapLost</name>
      <anchorfile>classsacn_1_1_receiver_1_1_notify_handler.html</anchorfile>
      <anchor>afc6fd518860704ac4b1f58d5a24f6672</anchor>
      <arglist>(Handle handle, uint16_t universe, const SacnRemoteSource &amp;source)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>HandleSourceLimitExceeded</name>
      <anchorfile>classsacn_1_1_receiver_1_1_notify_handler.html</anchorfile>
      <anchor>ab517496e89eb88b7ee7fa383558f725b</anchor>
      <arglist>(Handle handle, uint16_t universe)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>sacn::SourceDetector::NotifyHandler</name>
    <filename>classsacn_1_1_source_detector_1_1_notify_handler.html</filename>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>HandleSourceUpdated</name>
      <anchorfile>classsacn_1_1_source_detector_1_1_notify_handler.html</anchorfile>
      <anchor>ad449da95881e07e5566be76ebecd862d</anchor>
      <arglist>(RemoteSourceHandle handle, const etcpal::Uuid &amp;cid, const std::string &amp;name, const std::vector&lt; uint16_t &gt; &amp;sourced_universes)=0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>HandleSourceExpired</name>
      <anchorfile>classsacn_1_1_source_detector_1_1_notify_handler.html</anchorfile>
      <anchor>a2ed8374706af489c957a72c39cc760e0</anchor>
      <arglist>(RemoteSourceHandle handle, const etcpal::Uuid &amp;cid, const std::string &amp;name)=0</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>HandleMemoryLimitExceeded</name>
      <anchorfile>classsacn_1_1_source_detector_1_1_notify_handler.html</anchorfile>
      <anchor>a7e316ac3388e51f8a4d15e95495bc334</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>sacn::Receiver</name>
    <filename>classsacn_1_1_receiver.html</filename>
    <class kind="struct">sacn::Receiver::NetintList</class>
    <class kind="class">sacn::Receiver::NotifyHandler</class>
    <class kind="struct">sacn::Receiver::Settings</class>
    <member kind="typedef">
      <type>etcpal::OpaqueId&lt; detail::ReceiverHandleType, sacn_receiver_t, SACN_RECEIVER_INVALID &gt;</type>
      <name>Handle</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>a7337fe02f7aa0f984a2da3fe2c7d4524</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>Receiver</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>a8b226a7db315d39b6fdc4c8e61c3d896</anchor>
      <arglist>(Receiver &amp;&amp;other)=default</arglist>
    </member>
    <member kind="function">
      <type>Receiver &amp;</type>
      <name>operator=</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>a9312079cda8a73e44f853bd361239f39</anchor>
      <arglist>(Receiver &amp;&amp;other)=default</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Startup</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>a1fa0e47dc34289f2cffabdfc69c216a2</anchor>
      <arglist>(const Settings &amp;settings, NotifyHandler &amp;notify_handler, McastMode mcast_mode)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Startup</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>abb65535d2f1ed30a79014d81eb7f6807</anchor>
      <arglist>(const Settings &amp;settings, NotifyHandler &amp;notify_handler, std::vector&lt; SacnMcastInterface &gt; &amp;netints)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Shutdown</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>ac5f038c2b480cf9ef5e19e3eba8dbaf9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Expected&lt; uint16_t &gt;</type>
      <name>GetUniverse</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>a3d205f22d3d18f81585fd8d07ea21647</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Expected&lt; SacnRecvUniverseSubrange &gt;</type>
      <name>GetFootprint</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>aaa4d2448f1e1f8b23c095742738df97c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>ChangeUniverse</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>aa94ae64c11fbb22db3e21fba1e85cc64</anchor>
      <arglist>(uint16_t new_universe_id)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>ChangeFootprint</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>a6d46635ac84bcb47ff0d01bcafe4ef91</anchor>
      <arglist>(const SacnRecvUniverseSubrange &amp;new_footprint)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>ChangeUniverseAndFootprint</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>ac097835505ac503b525bcaa566cf506d</anchor>
      <arglist>(uint16_t new_universe_id, const SacnRecvUniverseSubrange &amp;new_footprint)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; EtcPalMcastNetintId &gt;</type>
      <name>GetNetworkInterfaces</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>a227c8165a2af9b39ac6d53cbea18d121</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>constexpr Handle</type>
      <name>handle</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>af5825bc604b5bf0316732a4db412e1f8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>SetExpiredWait</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>a0f7e0ac39dc43d67c53daa787e733859</anchor>
      <arglist>(uint32_t wait_ms)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static uint32_t</type>
      <name>GetExpiredWait</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>a042c03773822b8aeada4ea92ee3c3a8a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static etcpal::Error</type>
      <name>ResetNetworking</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>aab11585b71f32bd1973f307435ffcd5b</anchor>
      <arglist>(McastMode mcast_mode)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static etcpal::Error</type>
      <name>ResetNetworking</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>a26dd2b3b95dbaac498bfc0ba231f42e6</anchor>
      <arglist>(std::vector&lt; SacnMcastInterface &gt; &amp;netints)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static etcpal::Error</type>
      <name>ResetNetworking</name>
      <anchorfile>classsacn_1_1_receiver.html</anchorfile>
      <anchor>a8c0764f462388260aa5a6ab3fec2359c</anchor>
      <arglist>(std::vector&lt; SacnMcastInterface &gt; &amp;sys_netints, std::vector&lt; NetintList &gt; &amp;netint_lists)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>sacn::detail::ReceiverHandleType</name>
    <filename>classsacn_1_1detail_1_1_receiver_handle_type.html</filename>
  </compound>
  <compound kind="struct">
    <name>SacnDmxMergerConfig</name>
    <filename>struct_sacn_dmx_merger_config.html</filename>
    <member kind="variable">
      <type>uint8_t *</type>
      <name>levels</name>
      <anchorfile>struct_sacn_dmx_merger_config.html</anchorfile>
      <anchor>a65f1e6db75de6e20f9df99807d14fe19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t *</type>
      <name>per_address_priorities</name>
      <anchorfile>struct_sacn_dmx_merger_config.html</anchorfile>
      <anchor>a6b8790b29687764423319cc2b8f24c30</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool *</type>
      <name>per_address_priorities_active</name>
      <anchorfile>struct_sacn_dmx_merger_config.html</anchorfile>
      <anchor>aa844a92b4e32e58dee13b77318f8cb8e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t *</type>
      <name>universe_priority</name>
      <anchorfile>struct_sacn_dmx_merger_config.html</anchorfile>
      <anchor>ae39fb52602dd51d0696c8973b0d5cd44</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>sacn_dmx_merger_source_t *</type>
      <name>owners</name>
      <anchorfile>struct_sacn_dmx_merger_config.html</anchorfile>
      <anchor>aec37c8bd38cd533fbff8d704cef812d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>source_count_max</name>
      <anchorfile>struct_sacn_dmx_merger_config.html</anchorfile>
      <anchor>aa56795d5b57bff4910ce11baac4003ba</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnDmxMergerSource</name>
    <filename>struct_sacn_dmx_merger_source.html</filename>
    <member kind="variable">
      <type>sacn_dmx_merger_source_t</type>
      <name>id</name>
      <anchorfile>struct_sacn_dmx_merger_source.html</anchorfile>
      <anchor>a5d30b5a537ddb24760bd71590427ac3e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>levels</name>
      <anchorfile>struct_sacn_dmx_merger_source.html</anchorfile>
      <anchor>a82f491441fd8ea7e0b36c867797a9ba5</anchor>
      <arglist>[DMX_ADDRESS_COUNT]</arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>valid_level_count</name>
      <anchorfile>struct_sacn_dmx_merger_source.html</anchorfile>
      <anchor>a3695ed7e336fa1e6cf75ee3ccbc4937d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>universe_priority</name>
      <anchorfile>struct_sacn_dmx_merger_source.html</anchorfile>
      <anchor>aefc54b6cd291e480d6cb7c9b5b98a3f4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>address_priority</name>
      <anchorfile>struct_sacn_dmx_merger_source.html</anchorfile>
      <anchor>a8e2ad703e06552317b7f47dd0e0ebbea</anchor>
      <arglist>[DMX_ADDRESS_COUNT]</arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>using_universe_priority</name>
      <anchorfile>struct_sacn_dmx_merger_source.html</anchorfile>
      <anchor>a61802cbe97a2c1aa336df0282b2bb07f</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnLostSource</name>
    <filename>struct_sacn_lost_source.html</filename>
    <member kind="variable">
      <type>sacn_remote_source_t</type>
      <name>handle</name>
      <anchorfile>struct_sacn_lost_source.html</anchorfile>
      <anchor>abd2d36f012ca716885c43720824b4a75</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>EtcPalUuid</type>
      <name>cid</name>
      <anchorfile>struct_sacn_lost_source.html</anchorfile>
      <anchor>a4c3f7d9a58af5033f9acd8c942bf81d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>name</name>
      <anchorfile>struct_sacn_lost_source.html</anchorfile>
      <anchor>a7e3669a12853b82f7d2b7488bb174956</anchor>
      <arglist>[SACN_SOURCE_NAME_MAX_LEN]</arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>terminated</name>
      <anchorfile>struct_sacn_lost_source.html</anchorfile>
      <anchor>ad1aea42fae01ba0d2917114d189a9d36</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnMcastInterface</name>
    <filename>struct_sacn_mcast_interface.html</filename>
    <member kind="variable">
      <type>EtcPalMcastNetintId</type>
      <name>iface</name>
      <anchorfile>struct_sacn_mcast_interface.html</anchorfile>
      <anchor>a9d398e27f9eb3b1ee3b422225196b684</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>etcpal_error_t</type>
      <name>status</name>
      <anchorfile>struct_sacn_mcast_interface.html</anchorfile>
      <anchor>a3ed2ca570213574abb27e4d16cab480b</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnMergeReceiverCallbacks</name>
    <filename>struct_sacn_merge_receiver_callbacks.html</filename>
    <member kind="variable">
      <type>SacnMergeReceiverMergedDataCallback</type>
      <name>universe_data</name>
      <anchorfile>struct_sacn_merge_receiver_callbacks.html</anchorfile>
      <anchor>a1a4295429c025c42d5a54b646f03f62b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnMergeReceiverNonDmxCallback</type>
      <name>universe_non_dmx</name>
      <anchorfile>struct_sacn_merge_receiver_callbacks.html</anchorfile>
      <anchor>ace7f60e7140c695d729d045abca2ba73</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnMergeReceiverSourcesLostCallback</type>
      <name>sources_lost</name>
      <anchorfile>struct_sacn_merge_receiver_callbacks.html</anchorfile>
      <anchor>ad26d911447fbf0732edb4c48e6cdd043</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnMergeReceiverSamplingPeriodStartedCallback</type>
      <name>sampling_period_started</name>
      <anchorfile>struct_sacn_merge_receiver_callbacks.html</anchorfile>
      <anchor>a25de9ebaf3bbe659df822828f4011b42</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnMergeReceiverSamplingPeriodEndedCallback</type>
      <name>sampling_period_ended</name>
      <anchorfile>struct_sacn_merge_receiver_callbacks.html</anchorfile>
      <anchor>ab5b543dcbe601f5b9ae53cdb1564fa3d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnMergeReceiverSourcePapLostCallback</type>
      <name>source_pap_lost</name>
      <anchorfile>struct_sacn_merge_receiver_callbacks.html</anchorfile>
      <anchor>a78c717807daa436372d67d4c2a43a354</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnMergeReceiverSourceLimitExceededCallback</type>
      <name>source_limit_exceeded</name>
      <anchorfile>struct_sacn_merge_receiver_callbacks.html</anchorfile>
      <anchor>a29e01e068c71bd55dd31d6aeb5483992</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>callback_context</name>
      <anchorfile>struct_sacn_merge_receiver_callbacks.html</anchorfile>
      <anchor>a67b5953b36108e04f24abe803bc1fe44</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnMergeReceiverConfig</name>
    <filename>struct_sacn_merge_receiver_config.html</filename>
    <member kind="variable">
      <type>uint16_t</type>
      <name>universe_id</name>
      <anchorfile>struct_sacn_merge_receiver_config.html</anchorfile>
      <anchor>a7e0de4d0f4e4bbd14771257febdf7248</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnMergeReceiverCallbacks</type>
      <name>callbacks</name>
      <anchorfile>struct_sacn_merge_receiver_config.html</anchorfile>
      <anchor>ac1381db71af830c6635c5d2b338217ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnRecvUniverseSubrange</type>
      <name>footprint</name>
      <anchorfile>struct_sacn_merge_receiver_config.html</anchorfile>
      <anchor>a3fa13922bd1ee44412886d8e562582c8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>source_count_max</name>
      <anchorfile>struct_sacn_merge_receiver_config.html</anchorfile>
      <anchor>aa56795d5b57bff4910ce11baac4003ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>use_pap</name>
      <anchorfile>struct_sacn_merge_receiver_config.html</anchorfile>
      <anchor>af4536f740a72ae5788afacd0db91da30</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>sacn_ip_support_t</type>
      <name>ip_supported</name>
      <anchorfile>struct_sacn_merge_receiver_config.html</anchorfile>
      <anchor>ab619351af6b7a5abcb23e38bdc09e984</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnMergeReceiverNetintList</name>
    <filename>struct_sacn_merge_receiver_netint_list.html</filename>
    <member kind="variable">
      <type>sacn_merge_receiver_t</type>
      <name>handle</name>
      <anchorfile>struct_sacn_merge_receiver_netint_list.html</anchorfile>
      <anchor>aba7e88ef8eda8612d97729978383ba05</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnMcastInterface *</type>
      <name>netints</name>
      <anchorfile>struct_sacn_merge_receiver_netint_list.html</anchorfile>
      <anchor>add0da5313d4b1d06a67bda5508b848e1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>num_netints</name>
      <anchorfile>struct_sacn_merge_receiver_netint_list.html</anchorfile>
      <anchor>a6ddc029bb8aba3ead075508554592445</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>no_netints</name>
      <anchorfile>struct_sacn_merge_receiver_netint_list.html</anchorfile>
      <anchor>a45fc59834ac15fd6d3c71454af1d247a</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnMergeReceiverSource</name>
    <filename>struct_sacn_merge_receiver_source.html</filename>
    <member kind="variable">
      <type>sacn_remote_source_t</type>
      <name>handle</name>
      <anchorfile>struct_sacn_merge_receiver_source.html</anchorfile>
      <anchor>abd2d36f012ca716885c43720824b4a75</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>EtcPalUuid</type>
      <name>cid</name>
      <anchorfile>struct_sacn_merge_receiver_source.html</anchorfile>
      <anchor>a4c3f7d9a58af5033f9acd8c942bf81d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>name</name>
      <anchorfile>struct_sacn_merge_receiver_source.html</anchorfile>
      <anchor>a7e3669a12853b82f7d2b7488bb174956</anchor>
      <arglist>[SACN_SOURCE_NAME_MAX_LEN]</arglist>
    </member>
    <member kind="variable">
      <type>EtcPalSockAddr</type>
      <name>addr</name>
      <anchorfile>struct_sacn_merge_receiver_source.html</anchorfile>
      <anchor>a730cebe0970ed6300a5f832363e65852</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>per_address_priorities_active</name>
      <anchorfile>struct_sacn_merge_receiver_source.html</anchorfile>
      <anchor>a401cf1a9b9e5bd901e1600ea9abc4350</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>universe_priority</name>
      <anchorfile>struct_sacn_merge_receiver_source.html</anchorfile>
      <anchor>aefc54b6cd291e480d6cb7c9b5b98a3f4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnNetintConfig</name>
    <filename>struct_sacn_netint_config.html</filename>
    <member kind="variable">
      <type>SacnMcastInterface *</type>
      <name>netints</name>
      <anchorfile>struct_sacn_netint_config.html</anchorfile>
      <anchor>add0da5313d4b1d06a67bda5508b848e1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>num_netints</name>
      <anchorfile>struct_sacn_netint_config.html</anchorfile>
      <anchor>a6ddc029bb8aba3ead075508554592445</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>no_netints</name>
      <anchorfile>struct_sacn_netint_config.html</anchorfile>
      <anchor>a45fc59834ac15fd6d3c71454af1d247a</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnReceiverCallbacks</name>
    <filename>struct_sacn_receiver_callbacks.html</filename>
    <member kind="variable">
      <type>SacnUniverseDataCallback</type>
      <name>universe_data</name>
      <anchorfile>struct_sacn_receiver_callbacks.html</anchorfile>
      <anchor>a98b4132d20211455c32c7d1d73b1e156</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnSourcesLostCallback</type>
      <name>sources_lost</name>
      <anchorfile>struct_sacn_receiver_callbacks.html</anchorfile>
      <anchor>a4aa47bcca0d30a2c28463942cac94be5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnSamplingPeriodStartedCallback</type>
      <name>sampling_period_started</name>
      <anchorfile>struct_sacn_receiver_callbacks.html</anchorfile>
      <anchor>a401d2be24423a263467e2b7b09f68b0b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnSamplingPeriodEndedCallback</type>
      <name>sampling_period_ended</name>
      <anchorfile>struct_sacn_receiver_callbacks.html</anchorfile>
      <anchor>a3c827e69c782db7012f8d20f9a25bf53</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnSourcePapLostCallback</type>
      <name>source_pap_lost</name>
      <anchorfile>struct_sacn_receiver_callbacks.html</anchorfile>
      <anchor>ae587c65553885a5286a270d7bca267dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnSourceLimitExceededCallback</type>
      <name>source_limit_exceeded</name>
      <anchorfile>struct_sacn_receiver_callbacks.html</anchorfile>
      <anchor>addfb88eea24f335ebbfc61590a5dffd3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>context</name>
      <anchorfile>struct_sacn_receiver_callbacks.html</anchorfile>
      <anchor>ae376f130b17d169ee51be68077a89ed0</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnReceiverConfig</name>
    <filename>struct_sacn_receiver_config.html</filename>
    <member kind="variable">
      <type>uint16_t</type>
      <name>universe_id</name>
      <anchorfile>struct_sacn_receiver_config.html</anchorfile>
      <anchor>a7e0de4d0f4e4bbd14771257febdf7248</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnReceiverCallbacks</type>
      <name>callbacks</name>
      <anchorfile>struct_sacn_receiver_config.html</anchorfile>
      <anchor>a4beb413a7e2291bf1488fc6472af6aa4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnRecvUniverseSubrange</type>
      <name>footprint</name>
      <anchorfile>struct_sacn_receiver_config.html</anchorfile>
      <anchor>a3fa13922bd1ee44412886d8e562582c8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>source_count_max</name>
      <anchorfile>struct_sacn_receiver_config.html</anchorfile>
      <anchor>aa56795d5b57bff4910ce11baac4003ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned int</type>
      <name>flags</name>
      <anchorfile>struct_sacn_receiver_config.html</anchorfile>
      <anchor>ac92588540e8c1d014a08cd8a45462b19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>sacn_ip_support_t</type>
      <name>ip_supported</name>
      <anchorfile>struct_sacn_receiver_config.html</anchorfile>
      <anchor>ab619351af6b7a5abcb23e38bdc09e984</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnReceiverNetintList</name>
    <filename>struct_sacn_receiver_netint_list.html</filename>
    <member kind="variable">
      <type>sacn_receiver_t</type>
      <name>handle</name>
      <anchorfile>struct_sacn_receiver_netint_list.html</anchorfile>
      <anchor>a0d237fd6d38af257c8ab2a14a64a76e0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnMcastInterface *</type>
      <name>netints</name>
      <anchorfile>struct_sacn_receiver_netint_list.html</anchorfile>
      <anchor>add0da5313d4b1d06a67bda5508b848e1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>num_netints</name>
      <anchorfile>struct_sacn_receiver_netint_list.html</anchorfile>
      <anchor>a6ddc029bb8aba3ead075508554592445</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>no_netints</name>
      <anchorfile>struct_sacn_receiver_netint_list.html</anchorfile>
      <anchor>a45fc59834ac15fd6d3c71454af1d247a</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnRecvMergedData</name>
    <filename>struct_sacn_recv_merged_data.html</filename>
    <member kind="variable">
      <type>uint16_t</type>
      <name>universe_id</name>
      <anchorfile>struct_sacn_recv_merged_data.html</anchorfile>
      <anchor>a7e0de4d0f4e4bbd14771257febdf7248</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnRecvUniverseSubrange</type>
      <name>slot_range</name>
      <anchorfile>struct_sacn_recv_merged_data.html</anchorfile>
      <anchor>ad634e09aa77487d44087c02a9143ee17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const uint8_t *</type>
      <name>levels</name>
      <anchorfile>struct_sacn_recv_merged_data.html</anchorfile>
      <anchor>a538119b446f2e353678608bd05ad4db2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const uint8_t *</type>
      <name>priorities</name>
      <anchorfile>struct_sacn_recv_merged_data.html</anchorfile>
      <anchor>a5c59c7a7fc70b6d32442895898e7d55e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const sacn_remote_source_t *</type>
      <name>owners</name>
      <anchorfile>struct_sacn_recv_merged_data.html</anchorfile>
      <anchor>ade95d8642c07e44ecacc13f358ede54d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const sacn_remote_source_t *</type>
      <name>active_sources</name>
      <anchorfile>struct_sacn_recv_merged_data.html</anchorfile>
      <anchor>a067998056b2591fd39a6ab1247edf94b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>num_active_sources</name>
      <anchorfile>struct_sacn_recv_merged_data.html</anchorfile>
      <anchor>a6f445f896ce37297dbcd575006fdcb6c</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnRecvUniverseData</name>
    <filename>struct_sacn_recv_universe_data.html</filename>
    <member kind="variable">
      <type>uint16_t</type>
      <name>universe_id</name>
      <anchorfile>struct_sacn_recv_universe_data.html</anchorfile>
      <anchor>a7e0de4d0f4e4bbd14771257febdf7248</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>priority</name>
      <anchorfile>struct_sacn_recv_universe_data.html</anchorfile>
      <anchor>a0ad043071ccc7a261d79a759dc9c6f0c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>preview</name>
      <anchorfile>struct_sacn_recv_universe_data.html</anchorfile>
      <anchor>a631d1206624dc91b6c574bf919a7699d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_sampling</name>
      <anchorfile>struct_sacn_recv_universe_data.html</anchorfile>
      <anchor>a7d039abc15e5b41fb9404c941d705648</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>start_code</name>
      <anchorfile>struct_sacn_recv_universe_data.html</anchorfile>
      <anchor>a09df16493b12c991aef309819c8eebab</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnRecvUniverseSubrange</type>
      <name>slot_range</name>
      <anchorfile>struct_sacn_recv_universe_data.html</anchorfile>
      <anchor>ad634e09aa77487d44087c02a9143ee17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const uint8_t *</type>
      <name>values</name>
      <anchorfile>struct_sacn_recv_universe_data.html</anchorfile>
      <anchor>a92142b0edf6b988306f478760c7a38aa</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnRecvUniverseSubrange</name>
    <filename>struct_sacn_recv_universe_subrange.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>start_address</name>
      <anchorfile>struct_sacn_recv_universe_subrange.html</anchorfile>
      <anchor>ace16bae043b38ea02cb084b12bcbf646</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>address_count</name>
      <anchorfile>struct_sacn_recv_universe_subrange.html</anchorfile>
      <anchor>a827b7214573943430871533037d0f5b9</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnRemoteSource</name>
    <filename>struct_sacn_remote_source.html</filename>
    <member kind="variable">
      <type>sacn_remote_source_t</type>
      <name>handle</name>
      <anchorfile>struct_sacn_remote_source.html</anchorfile>
      <anchor>abd2d36f012ca716885c43720824b4a75</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>EtcPalUuid</type>
      <name>cid</name>
      <anchorfile>struct_sacn_remote_source.html</anchorfile>
      <anchor>a4c3f7d9a58af5033f9acd8c942bf81d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>name</name>
      <anchorfile>struct_sacn_remote_source.html</anchorfile>
      <anchor>a7e3669a12853b82f7d2b7488bb174956</anchor>
      <arglist>[SACN_SOURCE_NAME_MAX_LEN]</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnSourceConfig</name>
    <filename>struct_sacn_source_config.html</filename>
    <member kind="variable">
      <type>EtcPalUuid</type>
      <name>cid</name>
      <anchorfile>struct_sacn_source_config.html</anchorfile>
      <anchor>a4c3f7d9a58af5033f9acd8c942bf81d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchorfile>struct_sacn_source_config.html</anchorfile>
      <anchor>a8f8f80d37794cde9472343e4487ba3eb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>universe_count_max</name>
      <anchorfile>struct_sacn_source_config.html</anchorfile>
      <anchor>af4a1d1d970b94a6a07b8c612b8d7ad73</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>manually_process_source</name>
      <anchorfile>struct_sacn_source_config.html</anchorfile>
      <anchor>a16ca9a84fb49dbc5b40b8b333a239e2d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>sacn_ip_support_t</type>
      <name>ip_supported</name>
      <anchorfile>struct_sacn_source_config.html</anchorfile>
      <anchor>ab619351af6b7a5abcb23e38bdc09e984</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>keep_alive_interval</name>
      <anchorfile>struct_sacn_source_config.html</anchorfile>
      <anchor>a987c78ebef19e1addb7485ce29ef4dd7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>pap_keep_alive_interval</name>
      <anchorfile>struct_sacn_source_config.html</anchorfile>
      <anchor>aa0c6c9ee861a74ce7f88cbf8ba7ec99f</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnSourceDetectorCallbacks</name>
    <filename>struct_sacn_source_detector_callbacks.html</filename>
    <member kind="variable">
      <type>SacnSourceDetectorSourceUpdatedCallback</type>
      <name>source_updated</name>
      <anchorfile>struct_sacn_source_detector_callbacks.html</anchorfile>
      <anchor>ae4d714b0bc64241d5632fe77d013fb45</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnSourceDetectorSourceExpiredCallback</type>
      <name>source_expired</name>
      <anchorfile>struct_sacn_source_detector_callbacks.html</anchorfile>
      <anchor>ad53b703dc988168f5b0b5bff06b2d2ff</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnSourceDetectorLimitExceededCallback</type>
      <name>limit_exceeded</name>
      <anchorfile>struct_sacn_source_detector_callbacks.html</anchorfile>
      <anchor>a2a102ec50557c2902e0b43656c0fa33c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>context</name>
      <anchorfile>struct_sacn_source_detector_callbacks.html</anchorfile>
      <anchor>ae376f130b17d169ee51be68077a89ed0</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnSourceDetectorConfig</name>
    <filename>struct_sacn_source_detector_config.html</filename>
    <member kind="variable">
      <type>SacnSourceDetectorCallbacks</type>
      <name>callbacks</name>
      <anchorfile>struct_sacn_source_detector_config.html</anchorfile>
      <anchor>ac7495377d16d49972f79cebbd69f7095</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>source_count_max</name>
      <anchorfile>struct_sacn_source_detector_config.html</anchorfile>
      <anchor>aa56795d5b57bff4910ce11baac4003ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>universes_per_source_max</name>
      <anchorfile>struct_sacn_source_detector_config.html</anchorfile>
      <anchor>a56cecc9bab9fb8d197ce21bbbc036e06</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>sacn_ip_support_t</type>
      <name>ip_supported</name>
      <anchorfile>struct_sacn_source_detector_config.html</anchorfile>
      <anchor>ab619351af6b7a5abcb23e38bdc09e984</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnSourceUniverseConfig</name>
    <filename>struct_sacn_source_universe_config.html</filename>
    <member kind="variable">
      <type>uint16_t</type>
      <name>universe</name>
      <anchorfile>struct_sacn_source_universe_config.html</anchorfile>
      <anchor>a0cf34824f8777f28b50f62b7870d6f8c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>priority</name>
      <anchorfile>struct_sacn_source_universe_config.html</anchorfile>
      <anchor>a0ad043071ccc7a261d79a759dc9c6f0c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>send_preview</name>
      <anchorfile>struct_sacn_source_universe_config.html</anchorfile>
      <anchor>a5c7913b52f7d575e1cedacbd6601b047</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>send_unicast_only</name>
      <anchorfile>struct_sacn_source_universe_config.html</anchorfile>
      <anchor>adcf08448ca5544a8be4518fc2d966ef0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const EtcPalIpAddr *</type>
      <name>unicast_destinations</name>
      <anchorfile>struct_sacn_source_universe_config.html</anchorfile>
      <anchor>a42ef84da1b2dc4f0e993432d4a7be8a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>num_unicast_destinations</name>
      <anchorfile>struct_sacn_source_universe_config.html</anchorfile>
      <anchor>aa148fe1bdf89b3e7b900a130d32de29c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint16_t</type>
      <name>sync_universe</name>
      <anchorfile>struct_sacn_source_universe_config.html</anchorfile>
      <anchor>a9c7dff151ba6b32fc343e6aa55147bcc</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SacnSourceUniverseNetintList</name>
    <filename>struct_sacn_source_universe_netint_list.html</filename>
    <member kind="variable">
      <type>sacn_source_t</type>
      <name>handle</name>
      <anchorfile>struct_sacn_source_universe_netint_list.html</anchorfile>
      <anchor>aee044b8551b82b1058427bbb17e74528</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint16_t</type>
      <name>universe</name>
      <anchorfile>struct_sacn_source_universe_netint_list.html</anchorfile>
      <anchor>a0cf34824f8777f28b50f62b7870d6f8c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnMcastInterface *</type>
      <name>netints</name>
      <anchorfile>struct_sacn_source_universe_netint_list.html</anchorfile>
      <anchor>add0da5313d4b1d06a67bda5508b848e1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>num_netints</name>
      <anchorfile>struct_sacn_source_universe_netint_list.html</anchorfile>
      <anchor>a6ddc029bb8aba3ead075508554592445</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>no_netints</name>
      <anchorfile>struct_sacn_source_universe_netint_list.html</anchorfile>
      <anchor>a45fc59834ac15fd6d3c71454af1d247a</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>sacn::DmxMerger::Settings</name>
    <filename>structsacn_1_1_dmx_merger_1_1_settings.html</filename>
    <member kind="function">
      <type></type>
      <name>Settings</name>
      <anchorfile>structsacn_1_1_dmx_merger_1_1_settings.html</anchorfile>
      <anchor>a408c38e2ece7d6a8dda04484f006e8bb</anchor>
      <arglist>()=default</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>Settings</name>
      <anchorfile>structsacn_1_1_dmx_merger_1_1_settings.html</anchorfile>
      <anchor>a22fb1d51b3d5315e1cd17f387e1abc77</anchor>
      <arglist>(uint8_t *levels_ptr)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsValid</name>
      <anchorfile>structsacn_1_1_dmx_merger_1_1_settings.html</anchorfile>
      <anchor>ac532c4b500b1a85ea22217f2c65a70ed</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable">
      <type>uint8_t *</type>
      <name>levels</name>
      <anchorfile>structsacn_1_1_dmx_merger_1_1_settings.html</anchorfile>
      <anchor>a65f1e6db75de6e20f9df99807d14fe19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t *</type>
      <name>per_address_priorities</name>
      <anchorfile>structsacn_1_1_dmx_merger_1_1_settings.html</anchorfile>
      <anchor>a6b8790b29687764423319cc2b8f24c30</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool *</type>
      <name>per_address_priorities_active</name>
      <anchorfile>structsacn_1_1_dmx_merger_1_1_settings.html</anchorfile>
      <anchor>aa844a92b4e32e58dee13b77318f8cb8e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t *</type>
      <name>universe_priority</name>
      <anchorfile>structsacn_1_1_dmx_merger_1_1_settings.html</anchorfile>
      <anchor>ae39fb52602dd51d0696c8973b0d5cd44</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>sacn_dmx_merger_source_t *</type>
      <name>owners</name>
      <anchorfile>structsacn_1_1_dmx_merger_1_1_settings.html</anchorfile>
      <anchor>aec37c8bd38cd533fbff8d704cef812d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>source_count_max</name>
      <anchorfile>structsacn_1_1_dmx_merger_1_1_settings.html</anchorfile>
      <anchor>aa56795d5b57bff4910ce11baac4003ba</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>sacn::MergeReceiver::Settings</name>
    <filename>structsacn_1_1_merge_receiver_1_1_settings.html</filename>
    <member kind="function">
      <type></type>
      <name>Settings</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_settings.html</anchorfile>
      <anchor>a408c38e2ece7d6a8dda04484f006e8bb</anchor>
      <arglist>()=default</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>Settings</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_settings.html</anchorfile>
      <anchor>a83831072af761dc03118f8474d28536f</anchor>
      <arglist>(uint16_t new_universe_id)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsValid</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_settings.html</anchorfile>
      <anchor>ac532c4b500b1a85ea22217f2c65a70ed</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable">
      <type>uint16_t</type>
      <name>universe_id</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_settings.html</anchorfile>
      <anchor>a7e0de4d0f4e4bbd14771257febdf7248</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnRecvUniverseSubrange</type>
      <name>footprint</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_settings.html</anchorfile>
      <anchor>a3fa13922bd1ee44412886d8e562582c8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>source_count_max</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_settings.html</anchorfile>
      <anchor>aa56795d5b57bff4910ce11baac4003ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>use_pap</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_settings.html</anchorfile>
      <anchor>af4536f740a72ae5788afacd0db91da30</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>sacn_ip_support_t</type>
      <name>ip_supported</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_settings.html</anchorfile>
      <anchor>ab619351af6b7a5abcb23e38bdc09e984</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>sacn::Receiver::Settings</name>
    <filename>structsacn_1_1_receiver_1_1_settings.html</filename>
    <member kind="function">
      <type></type>
      <name>Settings</name>
      <anchorfile>structsacn_1_1_receiver_1_1_settings.html</anchorfile>
      <anchor>a408c38e2ece7d6a8dda04484f006e8bb</anchor>
      <arglist>()=default</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>Settings</name>
      <anchorfile>structsacn_1_1_receiver_1_1_settings.html</anchorfile>
      <anchor>a83831072af761dc03118f8474d28536f</anchor>
      <arglist>(uint16_t new_universe_id)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsValid</name>
      <anchorfile>structsacn_1_1_receiver_1_1_settings.html</anchorfile>
      <anchor>ac532c4b500b1a85ea22217f2c65a70ed</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable">
      <type>uint16_t</type>
      <name>universe_id</name>
      <anchorfile>structsacn_1_1_receiver_1_1_settings.html</anchorfile>
      <anchor>a7e0de4d0f4e4bbd14771257febdf7248</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SacnRecvUniverseSubrange</type>
      <name>footprint</name>
      <anchorfile>structsacn_1_1_receiver_1_1_settings.html</anchorfile>
      <anchor>a3fa13922bd1ee44412886d8e562582c8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>source_count_max</name>
      <anchorfile>structsacn_1_1_receiver_1_1_settings.html</anchorfile>
      <anchor>aa56795d5b57bff4910ce11baac4003ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned int</type>
      <name>flags</name>
      <anchorfile>structsacn_1_1_receiver_1_1_settings.html</anchorfile>
      <anchor>ac92588540e8c1d014a08cd8a45462b19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>sacn_ip_support_t</type>
      <name>ip_supported</name>
      <anchorfile>structsacn_1_1_receiver_1_1_settings.html</anchorfile>
      <anchor>ab619351af6b7a5abcb23e38bdc09e984</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>sacn::Source::Settings</name>
    <filename>structsacn_1_1_source_1_1_settings.html</filename>
    <member kind="function">
      <type></type>
      <name>Settings</name>
      <anchorfile>structsacn_1_1_source_1_1_settings.html</anchorfile>
      <anchor>a408c38e2ece7d6a8dda04484f006e8bb</anchor>
      <arglist>()=default</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>Settings</name>
      <anchorfile>structsacn_1_1_source_1_1_settings.html</anchorfile>
      <anchor>a6a01ccea29e11babd7c2ba65931f7dc7</anchor>
      <arglist>(const etcpal::Uuid &amp;new_cid, const std::string &amp;new_name)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsValid</name>
      <anchorfile>structsacn_1_1_source_1_1_settings.html</anchorfile>
      <anchor>ac532c4b500b1a85ea22217f2c65a70ed</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable">
      <type>etcpal::Uuid</type>
      <name>cid</name>
      <anchorfile>structsacn_1_1_source_1_1_settings.html</anchorfile>
      <anchor>ab74b3ddda3c95279107d0059dd7ee9f3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>std::string</type>
      <name>name</name>
      <anchorfile>structsacn_1_1_source_1_1_settings.html</anchorfile>
      <anchor>a9b45b3e13bd9167aab02e17e08916231</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>universe_count_max</name>
      <anchorfile>structsacn_1_1_source_1_1_settings.html</anchorfile>
      <anchor>af4a1d1d970b94a6a07b8c612b8d7ad73</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>manually_process_source</name>
      <anchorfile>structsacn_1_1_source_1_1_settings.html</anchorfile>
      <anchor>a16ca9a84fb49dbc5b40b8b333a239e2d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>sacn_ip_support_t</type>
      <name>ip_supported</name>
      <anchorfile>structsacn_1_1_source_1_1_settings.html</anchorfile>
      <anchor>ab619351af6b7a5abcb23e38bdc09e984</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>keep_alive_interval</name>
      <anchorfile>structsacn_1_1_source_1_1_settings.html</anchorfile>
      <anchor>a987c78ebef19e1addb7485ce29ef4dd7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>pap_keep_alive_interval</name>
      <anchorfile>structsacn_1_1_source_1_1_settings.html</anchorfile>
      <anchor>aa0c6c9ee861a74ce7f88cbf8ba7ec99f</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>sacn::SourceDetector::Settings</name>
    <filename>structsacn_1_1_source_detector_1_1_settings.html</filename>
    <member kind="function">
      <type></type>
      <name>Settings</name>
      <anchorfile>structsacn_1_1_source_detector_1_1_settings.html</anchorfile>
      <anchor>a408c38e2ece7d6a8dda04484f006e8bb</anchor>
      <arglist>()=default</arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>source_count_max</name>
      <anchorfile>structsacn_1_1_source_detector_1_1_settings.html</anchorfile>
      <anchor>aa56795d5b57bff4910ce11baac4003ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>universes_per_source_max</name>
      <anchorfile>structsacn_1_1_source_detector_1_1_settings.html</anchorfile>
      <anchor>a56cecc9bab9fb8d197ce21bbbc036e06</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>sacn_ip_support_t</type>
      <name>ip_supported</name>
      <anchorfile>structsacn_1_1_source_detector_1_1_settings.html</anchorfile>
      <anchor>ab619351af6b7a5abcb23e38bdc09e984</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>sacn::MergeReceiver::Source</name>
    <filename>structsacn_1_1_merge_receiver_1_1_source.html</filename>
    <member kind="variable">
      <type>sacn_remote_source_t</type>
      <name>handle</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_source.html</anchorfile>
      <anchor>abd2d36f012ca716885c43720824b4a75</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>etcpal::Uuid</type>
      <name>cid</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_source.html</anchorfile>
      <anchor>ab74b3ddda3c95279107d0059dd7ee9f3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>std::string</type>
      <name>name</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_source.html</anchorfile>
      <anchor>a9b45b3e13bd9167aab02e17e08916231</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>etcpal::SockAddr</type>
      <name>addr</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_source.html</anchorfile>
      <anchor>a6089008b110eefe551612f1d3d9073bc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>per_address_priorities_active</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_source.html</anchorfile>
      <anchor>a401cf1a9b9e5bd901e1600ea9abc4350</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>universe_priority</name>
      <anchorfile>structsacn_1_1_merge_receiver_1_1_source.html</anchorfile>
      <anchor>aefc54b6cd291e480d6cb7c9b5b98a3f4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>sacn::Source</name>
    <filename>classsacn_1_1_source.html</filename>
    <class kind="struct">sacn::Source::Settings</class>
    <class kind="struct">sacn::Source::UniverseNetintList</class>
    <class kind="struct">sacn::Source::UniverseSettings</class>
    <member kind="enumeration">
      <type></type>
      <name>TickMode</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>af36a9f4e41fd0d0e2f1c538486bf57bf</anchor>
      <arglist></arglist>
      <enumvalue file="classsacn_1_1_source.html" anchor="af36a9f4e41fd0d0e2f1c538486bf57bfad95af72fafcf05564a73aa4c6ac8b79b">kProcessLevelsOnly</enumvalue>
      <enumvalue file="classsacn_1_1_source.html" anchor="af36a9f4e41fd0d0e2f1c538486bf57bfa8a07e62c29375b0e7b5c3cdc53158e2d">kProcessPapOnly</enumvalue>
      <enumvalue file="classsacn_1_1_source.html" anchor="af36a9f4e41fd0d0e2f1c538486bf57bfa3be842a0530fef755880cd131a19c6c5">kProcessLevelsAndPap</enumvalue>
    </member>
    <member kind="typedef">
      <type>etcpal::OpaqueId&lt; detail::SourceHandleType, sacn_source_t, SACN_SOURCE_INVALID &gt;</type>
      <name>Handle</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>a3defc9aed71824af85696f0f7e1b9ae4</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>Source</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>ab3745517fb270270e41c41f0938dc278</anchor>
      <arglist>(Source &amp;&amp;other)=default</arglist>
    </member>
    <member kind="function">
      <type>Source &amp;</type>
      <name>operator=</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>a70ca8a75288ff8b02f7674f8feea7940</anchor>
      <arglist>(Source &amp;&amp;other)=default</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Startup</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>ab263565edc9ebf5ffd6e82a18184e4c3</anchor>
      <arglist>(const Settings &amp;settings)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Shutdown</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>ac5f038c2b480cf9ef5e19e3eba8dbaf9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>ChangeName</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>aa33e199741640a65c41cc19d18b4b6f0</anchor>
      <arglist>(const std::string &amp;new_name)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>AddUniverse</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>a62069c900842c7ca322bd7ed4b587800</anchor>
      <arglist>(const UniverseSettings &amp;settings, McastMode mcast_mode)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>AddUniverse</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>acde56409812c209c488836f19cf53662</anchor>
      <arglist>(const UniverseSettings &amp;settings, std::vector&lt; SacnMcastInterface &gt; &amp;netints)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>RemoveUniverse</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>aa0293c6689075dd5cb9692f036c8540f</anchor>
      <arglist>(uint16_t universe)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; uint16_t &gt;</type>
      <name>GetUniverses</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>adc1d03b3f1516e7f545862b9096d834c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>AddUnicastDestination</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>a5d7f1f26d1a6f5dbffca60dd08ac0454</anchor>
      <arglist>(uint16_t universe, const etcpal::IpAddr &amp;dest)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>RemoveUnicastDestination</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>a2073e5814e81bf1257c7264a1b984966</anchor>
      <arglist>(uint16_t universe, const etcpal::IpAddr &amp;dest)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; etcpal::IpAddr &gt;</type>
      <name>GetUnicastDestinations</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>a80e1d17f72cc96da34ed4e2ed7b5a439</anchor>
      <arglist>(uint16_t universe)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>ChangePriority</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>a250e66ea54b315476f4c201600dd7b5f</anchor>
      <arglist>(uint16_t universe, uint8_t new_priority)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>ChangePreviewFlag</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>a386b243f62f7cc246de825add2af3161</anchor>
      <arglist>(uint16_t universe, bool new_preview_flag)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>ChangeSynchronizationUniverse</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>a6f73aa9c83e7d7266128cba5d4f70ef2</anchor>
      <arglist>(uint16_t universe, uint16_t new_sync_universe)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>SendNow</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>a22cf560eca13a3cbc4c8384bad438897</anchor>
      <arglist>(uint16_t universe, uint8_t start_code, const uint8_t *buffer, size_t buflen)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>SendSynchronization</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>aecaeb4e484f6b34e839b705a1395765f</anchor>
      <arglist>(uint16_t universe)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>UpdateLevels</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>acbec36dcb6b7ac017788510763dffef3</anchor>
      <arglist>(uint16_t universe, const uint8_t *new_levels, size_t new_levels_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>UpdateLevelsAndPap</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>a2d60e097ae578e5f8683fc10b271b126</anchor>
      <arglist>(uint16_t universe, const uint8_t *new_levels, size_t new_levels_size, const uint8_t *new_priorities, size_t new_priorities_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>UpdateLevelsAndForceSync</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>ab7249aefa54c1fc4a230439d08fcfddd</anchor>
      <arglist>(uint16_t universe, const uint8_t *new_levels, size_t new_levels_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>UpdateLevelsAndPapAndForceSync</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>ae6ab3c198ab506632e32e186bebe6a56</anchor>
      <arglist>(uint16_t universe, const uint8_t *new_levels, size_t new_levels_size, const uint8_t *new_priorities, size_t new_priorities_size)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; EtcPalMcastNetintId &gt;</type>
      <name>GetNetworkInterfaces</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>ab862dc0dd665318204b83182f016731c</anchor>
      <arglist>(uint16_t universe)</arglist>
    </member>
    <member kind="function">
      <type>constexpr Handle</type>
      <name>handle</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>a4f8c06497d45385dbb819d1dd4fc7a25</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static int</type>
      <name>ProcessManual</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>a90c818e6bc851e44b833014780ad939c</anchor>
      <arglist>(TickMode tick_mode)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static etcpal::Error</type>
      <name>ResetNetworking</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>aab11585b71f32bd1973f307435ffcd5b</anchor>
      <arglist>(McastMode mcast_mode)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static etcpal::Error</type>
      <name>ResetNetworking</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>a26dd2b3b95dbaac498bfc0ba231f42e6</anchor>
      <arglist>(std::vector&lt; SacnMcastInterface &gt; &amp;netints)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static etcpal::Error</type>
      <name>ResetNetworking</name>
      <anchorfile>classsacn_1_1_source.html</anchorfile>
      <anchor>af94950609d80afb351cf8e72a9f174c5</anchor>
      <arglist>(std::vector&lt; SacnMcastInterface &gt; &amp;sys_netints, std::vector&lt; UniverseNetintList &gt; &amp;netint_lists)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>sacn::SourceDetector</name>
    <filename>classsacn_1_1_source_detector.html</filename>
    <class kind="class">sacn::SourceDetector::NotifyHandler</class>
    <class kind="struct">sacn::SourceDetector::Settings</class>
    <member kind="function" static="yes">
      <type>static etcpal::Error</type>
      <name>Startup</name>
      <anchorfile>classsacn_1_1_source_detector.html</anchorfile>
      <anchor>a114885d14c950b73dde9f6168a809f8f</anchor>
      <arglist>(NotifyHandler &amp;notify_handler, McastMode mcast_mode)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static etcpal::Error</type>
      <name>Startup</name>
      <anchorfile>classsacn_1_1_source_detector.html</anchorfile>
      <anchor>a213169bd9a5d38cb80144f8d9fb82962</anchor>
      <arglist>(NotifyHandler &amp;notify_handler, std::vector&lt; SacnMcastInterface &gt; &amp;netints)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static etcpal::Error</type>
      <name>Startup</name>
      <anchorfile>classsacn_1_1_source_detector.html</anchorfile>
      <anchor>a1fa0e47dc34289f2cffabdfc69c216a2</anchor>
      <arglist>(const Settings &amp;settings, NotifyHandler &amp;notify_handler, McastMode mcast_mode)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static etcpal::Error</type>
      <name>Startup</name>
      <anchorfile>classsacn_1_1_source_detector.html</anchorfile>
      <anchor>abb65535d2f1ed30a79014d81eb7f6807</anchor>
      <arglist>(const Settings &amp;settings, NotifyHandler &amp;notify_handler, std::vector&lt; SacnMcastInterface &gt; &amp;netints)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>Shutdown</name>
      <anchorfile>classsacn_1_1_source_detector.html</anchorfile>
      <anchor>ac5f038c2b480cf9ef5e19e3eba8dbaf9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static etcpal::Error</type>
      <name>ResetNetworking</name>
      <anchorfile>classsacn_1_1_source_detector.html</anchorfile>
      <anchor>aab11585b71f32bd1973f307435ffcd5b</anchor>
      <arglist>(McastMode mcast_mode)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static etcpal::Error</type>
      <name>ResetNetworking</name>
      <anchorfile>classsacn_1_1_source_detector.html</anchorfile>
      <anchor>a26dd2b3b95dbaac498bfc0ba231f42e6</anchor>
      <arglist>(std::vector&lt; SacnMcastInterface &gt; &amp;netints)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static std::vector&lt; EtcPalMcastNetintId &gt;</type>
      <name>GetNetworkInterfaces</name>
      <anchorfile>classsacn_1_1_source_detector.html</anchorfile>
      <anchor>a227c8165a2af9b39ac6d53cbea18d121</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>sacn::detail::SourceHandleType</name>
    <filename>classsacn_1_1detail_1_1_source_handle_type.html</filename>
  </compound>
  <compound kind="struct">
    <name>sacn::Source::UniverseNetintList</name>
    <filename>structsacn_1_1_source_1_1_universe_netint_list.html</filename>
    <member kind="function">
      <type></type>
      <name>UniverseNetintList</name>
      <anchorfile>structsacn_1_1_source_1_1_universe_netint_list.html</anchorfile>
      <anchor>a73644ddd63df91014c177680f2336e47</anchor>
      <arglist>()=default</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>UniverseNetintList</name>
      <anchorfile>structsacn_1_1_source_1_1_universe_netint_list.html</anchorfile>
      <anchor>ad95505f4cce0aa335224a5680ca309f4</anchor>
      <arglist>(sacn_source_t source_handle, uint16_t universe_id, McastMode mcast_mode)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>UniverseNetintList</name>
      <anchorfile>structsacn_1_1_source_1_1_universe_netint_list.html</anchorfile>
      <anchor>aa0cef0ea55a0e9bc9d8a2c5ecd16ec31</anchor>
      <arglist>(sacn_source_t source_handle, uint16_t universe_id, const std::vector&lt; SacnMcastInterface &gt; &amp;network_interfaces)</arglist>
    </member>
    <member kind="variable">
      <type>sacn_source_t</type>
      <name>handle</name>
      <anchorfile>structsacn_1_1_source_1_1_universe_netint_list.html</anchorfile>
      <anchor>aee044b8551b82b1058427bbb17e74528</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint16_t</type>
      <name>universe</name>
      <anchorfile>structsacn_1_1_source_1_1_universe_netint_list.html</anchorfile>
      <anchor>a0cf34824f8777f28b50f62b7870d6f8c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>std::vector&lt; SacnMcastInterface &gt;</type>
      <name>netints</name>
      <anchorfile>structsacn_1_1_source_1_1_universe_netint_list.html</anchorfile>
      <anchor>a2a0cd7263bacb4663085d28599867b12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>no_netints</name>
      <anchorfile>structsacn_1_1_source_1_1_universe_netint_list.html</anchorfile>
      <anchor>a45fc59834ac15fd6d3c71454af1d247a</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>sacn::Source::UniverseSettings</name>
    <filename>structsacn_1_1_source_1_1_universe_settings.html</filename>
    <member kind="function">
      <type></type>
      <name>UniverseSettings</name>
      <anchorfile>structsacn_1_1_source_1_1_universe_settings.html</anchorfile>
      <anchor>af00507fb4c88058e9363bb4007ddea6f</anchor>
      <arglist>()=default</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>UniverseSettings</name>
      <anchorfile>structsacn_1_1_source_1_1_universe_settings.html</anchorfile>
      <anchor>aea1e029adb6a6702a0594a0a6fdce16d</anchor>
      <arglist>(uint16_t universe_id)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsValid</name>
      <anchorfile>structsacn_1_1_source_1_1_universe_settings.html</anchorfile>
      <anchor>ac532c4b500b1a85ea22217f2c65a70ed</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable">
      <type>uint16_t</type>
      <name>universe</name>
      <anchorfile>structsacn_1_1_source_1_1_universe_settings.html</anchorfile>
      <anchor>a0cf34824f8777f28b50f62b7870d6f8c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint8_t</type>
      <name>priority</name>
      <anchorfile>structsacn_1_1_source_1_1_universe_settings.html</anchorfile>
      <anchor>a0ad043071ccc7a261d79a759dc9c6f0c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>send_preview</name>
      <anchorfile>structsacn_1_1_source_1_1_universe_settings.html</anchorfile>
      <anchor>a5c7913b52f7d575e1cedacbd6601b047</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>send_unicast_only</name>
      <anchorfile>structsacn_1_1_source_1_1_universe_settings.html</anchorfile>
      <anchor>adcf08448ca5544a8be4518fc2d966ef0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>std::vector&lt; etcpal::IpAddr &gt;</type>
      <name>unicast_destinations</name>
      <anchorfile>structsacn_1_1_source_1_1_universe_settings.html</anchorfile>
      <anchor>a91fd2db41d13ddf5440d0d72619d1a2b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>uint16_t</type>
      <name>sync_universe</name>
      <anchorfile>structsacn_1_1_source_1_1_universe_settings.html</anchorfile>
      <anchor>a9c7dff151ba6b32fc343e6aa55147bcc</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>sacn</name>
    <filename>namespacesacn.html</filename>
    <class kind="class">sacn::DmxMerger</class>
    <class kind="class">sacn::MergeReceiver</class>
    <class kind="class">sacn::Receiver</class>
    <class kind="class">sacn::Source</class>
    <class kind="class">sacn::SourceDetector</class>
    <member kind="typedef">
      <type>sacn_remote_source_t</type>
      <name>RemoteSourceHandle</name>
      <anchorfile>namespacesacn.html</anchorfile>
      <anchor>acfd8aea0d62baa7d2f16a969ec5849af</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>McastMode</name>
      <anchorfile>namespacesacn.html</anchorfile>
      <anchor>af874a6c4f11432c529c4068e745889b5</anchor>
      <arglist></arglist>
      <enumvalue file="namespacesacn.html" anchor="af874a6c4f11432c529c4068e745889b5ad1989e9a06422a85d3d6d2ecf25a50cf">kEnabledOnAllInterfaces</enumvalue>
      <enumvalue file="namespacesacn.html" anchor="af874a6c4f11432c529c4068e745889b5a42f053f48441c9286254c451c8bea6a2">kDisabledOnAllInterfaces</enumvalue>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Init</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>gac1cf1cb5e698e8ad656481cc834925db</anchor>
      <arglist>(const EtcPalLogParams *log_params=nullptr, McastMode mcast_mode=McastMode::kEnabledOnAllInterfaces)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Init</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>ga462a83ab46a4fa6a2f642fcacabecc82</anchor>
      <arglist>(const EtcPalLogParams *log_params, std::vector&lt; SacnMcastInterface &gt; &amp;sys_netints)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Init</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>ga73781de46ab1321166a0dce24094c73d</anchor>
      <arglist>(std::vector&lt; SacnMcastInterface &gt; &amp;sys_netints)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Init</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>ga0dfea4ef6503c65fd4e13eb9813e6513</anchor>
      <arglist>(const etcpal::Logger &amp;logger, McastMode mcast_mode=McastMode::kEnabledOnAllInterfaces)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Init</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>ga414aefdf3a0364b6261374e61a37bd05</anchor>
      <arglist>(const etcpal::Logger &amp;logger, std::vector&lt; SacnMcastInterface &gt; &amp;sys_netints)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Deinit</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>ga7c27553e8de8ffb78e3627f51fe9eb25</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>RemoteSourceHandle</type>
      <name>GetRemoteSourceHandle</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>gaa353377fefbbdd6ae634d981944c5619</anchor>
      <arglist>(const etcpal::Uuid &amp;source_cid)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Expected&lt; etcpal::Uuid &gt;</type>
      <name>GetRemoteSourceCid</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>gab05243a382f6332a3d73dbb3b202bb95</anchor>
      <arglist>(RemoteSourceHandle source_handle)</arglist>
    </member>
    <member kind="variable">
      <type>constexpr RemoteSourceHandle</type>
      <name>kInvalidRemoteSourceHandle</name>
      <anchorfile>namespacesacn.html</anchorfile>
      <anchor>a731047fe50bd58178f7cd8b1960e6e57</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>sACN</name>
    <title>sACN</title>
    <filename>group__s_a_c_n.html</filename>
    <subgroup>sacn_dmx_merger</subgroup>
    <subgroup>sacn_merge_receiver</subgroup>
    <subgroup>sacn_receiver</subgroup>
    <subgroup>sacn_source</subgroup>
    <subgroup>sacn_source_detector</subgroup>
    <subgroup>sacnopts</subgroup>
    <class kind="struct">SacnMcastInterface</class>
    <class kind="struct">SacnNetintConfig</class>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_NAME_MAX_LEN</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga0a68bef69f737e31072b475521d331c8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DMX_ADDRESS_COUNT</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga681f92a30c76ae426e2403a328978abb</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_REMOTE_SOURCE_INVALID</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga4579b31d1f4e1ecff0ecd8214c551956</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_STARTCODE_DMX</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga2dba81c3bf923ae2dbb0aaa6d8d5fa0a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_STARTCODE_PRIORITY</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga92ab22221d9dedb5b22978e6c14c6349</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>uint16_t</type>
      <name>sacn_remote_source_t</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga31b1febd91134668307803d573ed2f2c</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnMcastInterface</type>
      <name>SacnMcastInterface</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>gabf46c3c353abbd956716fecbe24f2ae4</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnNetintConfig</type>
      <name>SacnNetintConfig</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga3cd2196005e33f66518a0f0baba34147</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>sacn_ip_support_t</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga02f82b9c734e2d2f70a1106d6480833a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kSacnIpV4Only</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>gga02f82b9c734e2d2f70a1106d6480833aa900230541148a1eb50b457dfbf75a3c4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kSacnIpV6Only</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>gga02f82b9c734e2d2f70a1106d6480833aaa7c7cce56d48e45e33bde272cb0be424</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kSacnIpV4AndIpV6</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>gga02f82b9c734e2d2f70a1106d6480833aa946f61a87ca52ca76b687484c5cdced2</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_init</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga612160fe1d0f1e4f1fae4d72232fee07</anchor>
      <arglist>(const EtcPalLogParams *log_params, const SacnNetintConfig *sys_netint_config)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_deinit</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga7b80ebcafe9eb3240a67785377872f9a</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>sacn_remote_source_t</type>
      <name>sacn_get_remote_source_handle</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga65d96208fc89676e2dea18d2ded31872</anchor>
      <arglist>(const EtcPalUuid *source_cid)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_get_remote_source_cid</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga9a71315342a299c22055fe195e6750ef</anchor>
      <arglist>(sacn_remote_source_t source_handle, EtcPalUuid *source_cid)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_MAJOR</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga3b04b863e88d1bae02133dfb19667e06</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_MINOR</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga234bcbd2198002c6ee8d3caa670ba0ed</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_PATCH</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>gae1d8849ebfa2d27cec433e54f7f7308d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_BUILD</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>gab22648c510945c218b806ad28e1e9a86</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_STRING</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga2ff6980847182dc1ac56ee3660e0a360</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_DATESTR</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga99a95e107267c6f80cc2195f86c11586</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_COPYRIGHT</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga5fd2e6c86426807d2eb598c67121723b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_VERSION_PRODUCTNAME</name>
      <anchorfile>group__s_a_c_n.html</anchorfile>
      <anchor>ga46b3c38236732e08243f8fe79bcf6c06</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>sacn_dmx_merger</name>
    <title>sACN DMX Merger</title>
    <filename>group__sacn__dmx__merger.html</filename>
    <class kind="struct">SacnDmxMergerConfig</class>
    <class kind="struct">SacnDmxMergerSource</class>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_INVALID</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga4578a59809c13ece174e8dcf59fce26d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_SOURCE_INVALID</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga1ca9e023e2091c5a8a4e5e2ba426f055</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_CONFIG_INIT</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga7431ab2baf3af7e0d1999355c6bcc9d7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_SOURCE_IS_VALID</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>gaec4962a6c5655b357d4c81dc9e7a7b86</anchor>
      <arglist>(owners_array, slot_index)</arglist>
    </member>
    <member kind="typedef">
      <type>int</type>
      <name>sacn_dmx_merger_t</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>gabee79fb378d5942866adc898cb7da38b</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>uint16_t</type>
      <name>sacn_dmx_merger_source_t</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga6c4761eedeaaf635ac495265849c07f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnDmxMergerConfig</type>
      <name>SacnDmxMergerConfig</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga9989e37ba8aa7aeb4ef0108ceb4e156c</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnDmxMergerSource</type>
      <name>SacnDmxMergerSource</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga5ff553cc00468871978edfe0e675aebd</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_dmx_merger_create</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga8758dad93531c1a5bbf4643157fe2c72</anchor>
      <arglist>(const SacnDmxMergerConfig *config, sacn_dmx_merger_t *handle)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_dmx_merger_destroy</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga539b249ea5d0898efce4ee7371fc91ef</anchor>
      <arglist>(sacn_dmx_merger_t handle)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_dmx_merger_add_source</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>gadf879eb673c0ffe91ade6bcc5af615c3</anchor>
      <arglist>(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t *source_id)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_dmx_merger_remove_source</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>gac6ebd9581fb8c6170d4acf1c24681ad7</anchor>
      <arglist>(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source)</arglist>
    </member>
    <member kind="function">
      <type>const SacnDmxMergerSource *</type>
      <name>sacn_dmx_merger_get_source</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga0811c88edb22c748c8b80bbc49984e67</anchor>
      <arglist>(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_dmx_merger_update_levels</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>ga6ee5e85689e75879fa1db01db0bad8a6</anchor>
      <arglist>(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source, const uint8_t *new_levels, size_t new_levels_count)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_dmx_merger_update_pap</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>gac28b72fd6849cb6815c0ca58c2d29e65</anchor>
      <arglist>(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source, const uint8_t *pap, size_t pap_count)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_dmx_merger_update_universe_priority</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>gab56b1f0e4fe8d5d1dd23695a1a861b3b</anchor>
      <arglist>(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source, uint8_t universe_priority)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_dmx_merger_remove_pap</name>
      <anchorfile>group__sacn__dmx__merger.html</anchorfile>
      <anchor>gada109b5171e78bc858ab18a7c13931ee</anchor>
      <arglist>(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source)</arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>sacn_merge_receiver</name>
    <title>sACN Merge Receiver</title>
    <filename>group__sacn__merge__receiver.html</filename>
    <class kind="struct">SacnRecvMergedData</class>
    <class kind="struct">SacnMergeReceiverCallbacks</class>
    <class kind="struct">SacnMergeReceiverConfig</class>
    <class kind="struct">SacnMergeReceiverNetintList</class>
    <class kind="struct">SacnMergeReceiverSource</class>
    <member kind="define">
      <type>#define</type>
      <name>SACN_MERGE_RECEIVER_INVALID</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>gae86374c92bace3a7d2bef9656da6048a</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_MERGE_RECEIVER_CONFIG_DEFAULT_INIT</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>gaeb89255ebd3592d97fe76ed6203913ae</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>int</type>
      <name>sacn_merge_receiver_t</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>gab674497f3bceb2d6ebf4b932b26bfe61</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnRecvMergedData</type>
      <name>SacnRecvMergedData</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>gafb05eeea1bbac03fd2058d197e2c5846</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnMergeReceiverMergedDataCallback</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga3b87b4a0324b476a926c86a18eab3a1d</anchor>
      <arglist>)(sacn_merge_receiver_t handle, const SacnRecvMergedData *merged_data, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnMergeReceiverNonDmxCallback</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga274346790ea25827de515b93f6df2c2b</anchor>
      <arglist>)(sacn_merge_receiver_t receiver_handle, const EtcPalSockAddr *source_addr, const SacnRemoteSource *source_info, const SacnRecvUniverseData *universe_data, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnMergeReceiverSourcesLostCallback</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga28f0d4119155beb794d94ef052ffe0c3</anchor>
      <arglist>)(sacn_merge_receiver_t handle, uint16_t universe, const SacnLostSource *lost_sources, size_t num_lost_sources, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnMergeReceiverSamplingPeriodStartedCallback</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga6bca38b03e63aa5a2e272449d23662ce</anchor>
      <arglist>)(sacn_merge_receiver_t handle, uint16_t universe, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnMergeReceiverSamplingPeriodEndedCallback</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga361d2c730abd2b490d99017f6ebb3739</anchor>
      <arglist>)(sacn_merge_receiver_t handle, uint16_t universe, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnMergeReceiverSourcePapLostCallback</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga3791403f2e07bd8df2364339f22d5b08</anchor>
      <arglist>)(sacn_merge_receiver_t handle, uint16_t universe, const SacnRemoteSource *source, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnMergeReceiverSourceLimitExceededCallback</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga47ccc08a4f5812e26ecf694fa0cb0ff0</anchor>
      <arglist>)(sacn_merge_receiver_t handle, uint16_t universe, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnMergeReceiverCallbacks</type>
      <name>SacnMergeReceiverCallbacks</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga92cb80327e80976e7b004bf9819bca0a</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnMergeReceiverConfig</type>
      <name>SacnMergeReceiverConfig</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga05a8525a8ee5280ab3b6e0583ae01ae8</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnMergeReceiverNetintList</type>
      <name>SacnMergeReceiverNetintList</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga2003a3a906229f10aaf77ad600ea1ad1</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnMergeReceiverSource</type>
      <name>SacnMergeReceiverSource</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>gac417e4563acfbf4bdfe8f2490c7e88c9</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_merge_receiver_config_init</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>gab0d6a624f1d44008625335112788eb37</anchor>
      <arglist>(SacnMergeReceiverConfig *config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_create</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga91ca26eea8b78eeb2b25bb003e8aa208</anchor>
      <arglist>(const SacnMergeReceiverConfig *config, sacn_merge_receiver_t *handle, const SacnNetintConfig *netint_config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_destroy</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga02c0879618435049e7a9bd25dbe58850</anchor>
      <arglist>(sacn_merge_receiver_t handle)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_get_universe</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga7345c390c8b94221b481c3cd1ca37de7</anchor>
      <arglist>(sacn_merge_receiver_t handle, uint16_t *universe_id)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_get_footprint</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga32dfa392c47b9f7a443e9bd563e1a974</anchor>
      <arglist>(sacn_merge_receiver_t handle, SacnRecvUniverseSubrange *footprint)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_change_universe</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga821122037b6cd927facd821c11cce970</anchor>
      <arglist>(sacn_merge_receiver_t handle, uint16_t new_universe_id)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_change_footprint</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga05a2a94b9c4346efa7918c7fe3031a80</anchor>
      <arglist>(sacn_merge_receiver_t handle, const SacnRecvUniverseSubrange *new_footprint)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_change_universe_and_footprint</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga99f34c7a019f2f4f5f62220a505c6e47</anchor>
      <arglist>(sacn_merge_receiver_t handle, uint16_t new_universe_id, const SacnRecvUniverseSubrange *new_footprint)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_reset_networking</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga00e28a2333ce8cd38b0eb3e58ac5f375</anchor>
      <arglist>(const SacnNetintConfig *sys_netint_config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_reset_networking_per_receiver</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga0db3ceb5a812f02e2e4daa52b5797523</anchor>
      <arglist>(const SacnNetintConfig *sys_netint_config, const SacnMergeReceiverNetintList *per_receiver_netint_lists, size_t num_per_receiver_netint_lists)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>sacn_merge_receiver_get_network_interfaces</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga01cecd351da958dc2fc7b55088559de8</anchor>
      <arglist>(sacn_merge_receiver_t handle, EtcPalMcastNetintId *netints, size_t netints_size)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_merge_receiver_get_source</name>
      <anchorfile>group__sacn__merge__receiver.html</anchorfile>
      <anchor>ga807515578955a36b0a33de53a1d5bd9c</anchor>
      <arglist>(sacn_merge_receiver_t merge_receiver_handle, sacn_remote_source_t source_handle, SacnMergeReceiverSource *source_info)</arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>sacn_receiver</name>
    <title>sACN Receiver</title>
    <filename>group__sacn__receiver.html</filename>
    <class kind="struct">SacnRecvUniverseSubrange</class>
    <class kind="struct">SacnRecvUniverseData</class>
    <class kind="struct">SacnRemoteSource</class>
    <class kind="struct">SacnLostSource</class>
    <class kind="struct">SacnReceiverCallbacks</class>
    <class kind="struct">SacnReceiverConfig</class>
    <class kind="struct">SacnReceiverNetintList</class>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_INVALID</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga79dd5d0d62fb4d6120290afeeadb3637</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_INFINITE_SOURCES</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gadb2ea19692692ca852423d0a9de749ea</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DEFAULT_EXPIRED_WAIT_MS</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga2f4617269e2d64c85b81556e0c3e8fde</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_CONFIG_DEFAULT_INIT</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gaaaa17f5e77d094f9348c0efd361cee52</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>int</type>
      <name>sacn_receiver_t</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gafc1e3c92911f567bed81bbd04f3f34f6</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnRecvUniverseSubrange</type>
      <name>SacnRecvUniverseSubrange</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gacd120c5410f4a19fe3faaeacbaaad904</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnRecvUniverseData</type>
      <name>SacnRecvUniverseData</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gac0c35d115d0d13af500b68eca8afda57</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnRemoteSource</type>
      <name>SacnRemoteSource</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga3fa6cd794c97a1c6c8330ec6b79aad38</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnLostSource</type>
      <name>SacnLostSource</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga7fdeb921a4c00c9821019145683bdda4</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnUniverseDataCallback</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga1ea1f87c1d1098d6df521cb9d9ccd0b3</anchor>
      <arglist>)(sacn_receiver_t receiver_handle, const EtcPalSockAddr *source_addr, const SacnRemoteSource *source_info, const SacnRecvUniverseData *universe_data, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnSourcesLostCallback</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga80326cc324898e1faebe2da1339bd0b3</anchor>
      <arglist>)(sacn_receiver_t handle, uint16_t universe, const SacnLostSource *lost_sources, size_t num_lost_sources, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnSamplingPeriodStartedCallback</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga5a47a2560a7aba67637d3a5b7ad22ef5</anchor>
      <arglist>)(sacn_receiver_t handle, uint16_t universe, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnSamplingPeriodEndedCallback</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gac9ac2e788bf69c6da712dfcd67269e67</anchor>
      <arglist>)(sacn_receiver_t handle, uint16_t universe, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnSourcePapLostCallback</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga7608a394e455465e56c789957f3f3214</anchor>
      <arglist>)(sacn_receiver_t handle, uint16_t universe, const SacnRemoteSource *source, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnSourceLimitExceededCallback</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga64bfe2ecfceca8b4f50f1a3a0a9c9a07</anchor>
      <arglist>)(sacn_receiver_t handle, uint16_t universe, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnReceiverCallbacks</type>
      <name>SacnReceiverCallbacks</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga114584027956aaaccfb9e74e1c311206</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnReceiverConfig</type>
      <name>SacnReceiverConfig</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gac54129e22da91ee8f1aaf9b56d771355</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnReceiverNetintList</type>
      <name>SacnReceiverNetintList</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga494aa6c2efd3b6a12943191c74946cb8</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_receiver_config_init</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga82f2e3740ce865b7aa3e018aa8a229d9</anchor>
      <arglist>(SacnReceiverConfig *config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_create</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gacc6321deb0647ac0175993be6083683b</anchor>
      <arglist>(const SacnReceiverConfig *config, sacn_receiver_t *handle, const SacnNetintConfig *netint_config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_destroy</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga43f99a2447364832c9ab135ac3d8b6ae</anchor>
      <arglist>(sacn_receiver_t handle)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_get_universe</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gae77eea445ebb380fb4d47c63c0f3ba32</anchor>
      <arglist>(sacn_receiver_t handle, uint16_t *universe_id)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_get_footprint</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga3dadba0fa169d0bfe82d5884004ecaa8</anchor>
      <arglist>(sacn_receiver_t handle, SacnRecvUniverseSubrange *footprint)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_change_universe</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gabb5ec7d4459ac694e5fa0ae97572f388</anchor>
      <arglist>(sacn_receiver_t handle, uint16_t new_universe_id)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_change_footprint</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gab5be9e77fa2652f71c93d9886ae826b2</anchor>
      <arglist>(sacn_receiver_t handle, const SacnRecvUniverseSubrange *new_footprint)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_change_universe_and_footprint</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gaf046362e62f8f4a910a00aaee3006247</anchor>
      <arglist>(sacn_receiver_t handle, uint16_t new_universe_id, const SacnRecvUniverseSubrange *new_footprint)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_reset_networking</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga54fd65585c71e83af35289c3cad0f685</anchor>
      <arglist>(const SacnNetintConfig *sys_netint_config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_receiver_reset_networking_per_receiver</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga29f330fc880d1c776afb5972514e6ac4</anchor>
      <arglist>(const SacnNetintConfig *sys_netint_config, const SacnReceiverNetintList *per_receiver_netint_lists, size_t num_per_receiver_netint_lists)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>sacn_receiver_get_network_interfaces</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga6ce5c1c8e4fafe2f339c6d3084dacfc4</anchor>
      <arglist>(sacn_receiver_t handle, EtcPalMcastNetintId *netints, size_t netints_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_receiver_set_expired_wait</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gabea27e71ae23c9176832f883a8ad7f06</anchor>
      <arglist>(uint32_t wait_ms)</arglist>
    </member>
    <member kind="function">
      <type>uint32_t</type>
      <name>sacn_receiver_get_expired_wait</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>gabd632f80d5da75c47c1f08103a42a391</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_OPTS_FILTER_PREVIEW_DATA</name>
      <anchorfile>group__sacn__receiver.html</anchorfile>
      <anchor>ga8613a5c435a6120a1d410bead3949087</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>sacn_source</name>
    <title>sACN Source</title>
    <filename>group__sacn__source.html</filename>
    <class kind="struct">SacnSourceConfig</class>
    <class kind="struct">SacnSourceUniverseConfig</class>
    <class kind="struct">SacnSourceUniverseNetintList</class>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_INVALID</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga446e9e065e08c7a90309a993b2502153</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_INFINITE_UNIVERSES</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga1ac5056bc752c32ca80c08b9839a142d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_KEEP_ALIVE_INTERVAL_DEFAULT</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gab3c248f42fcdeccba28617fd2612ce71</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_PAP_KEEP_ALIVE_INTERVAL_DEFAULT</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gae3df139429df836f977bab01da3af19d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_CONFIG_DEFAULT_INIT</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga1fc88ea9a51c4a935ec87630a0b177f1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga20483046bfb90da7c9bf089fc0229a6b</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>int</type>
      <name>sacn_source_t</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gacc4c9d2c77cf4126e9ac7faf297c3dd8</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnSourceConfig</type>
      <name>SacnSourceConfig</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gaf13f724dda8304930fcda0977088cd6a</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnSourceUniverseConfig</type>
      <name>SacnSourceUniverseConfig</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga3f72b89ed47da0e268c7735ae8971e89</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnSourceUniverseNetintList</type>
      <name>SacnSourceUniverseNetintList</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga0f30b2db7d3c6686a14bab97c80d73a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>sacn_source_tick_mode_t</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gaa32b392ec2a472f4b42d1395fa1d047d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kSacnSourceTickModeProcessLevelsOnly</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ggaa32b392ec2a472f4b42d1395fa1d047dad2e8539866e4da26bfaaad3f7fea354e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kSacnSourceTickModeProcessPapOnly</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ggaa32b392ec2a472f4b42d1395fa1d047daa135ff244f5bde7171ce9f53cb353280</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kSacnSourceTickModeProcessLevelsAndPap</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ggaa32b392ec2a472f4b42d1395fa1d047da811ce8e71a44eccfcf82135395792e61</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_config_init</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga4d115a7b828b0f45444ac36f0b946e64</anchor>
      <arglist>(SacnSourceConfig *config)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_universe_config_init</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga79d292e6589859e52e82834c611311ea</anchor>
      <arglist>(SacnSourceUniverseConfig *config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_create</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga37f43cd2ec265e3052eea6d5f1b68233</anchor>
      <arglist>(const SacnSourceConfig *config, sacn_source_t *handle)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_destroy</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gaeb8a9797368aabad696c81225b4c8aaa</anchor>
      <arglist>(sacn_source_t handle)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_change_name</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga0c44cd3dba2627779bfb65ea8349fe1c</anchor>
      <arglist>(sacn_source_t handle, const char *new_name)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_add_universe</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga22a80913e48354c6da37a9fa11626261</anchor>
      <arglist>(sacn_source_t handle, const SacnSourceUniverseConfig *config, const SacnNetintConfig *netint_config)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_remove_universe</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga7f8233fae115142c2e2a9488faa12d5d</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>sacn_source_get_universes</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga90b2e59fa6709ee72042778a60c93330</anchor>
      <arglist>(sacn_source_t handle, uint16_t *universes, size_t universes_size)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_add_unicast_destination</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga2b7d3fb6a2d252eee7c051acfe97d242</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr *dest)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_remove_unicast_destination</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga4ebe5d509af8c963c104f2dd96a3ac76</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr *dest)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>sacn_source_get_unicast_destinations</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga12f792b85add6af90ba2560f287b630c</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, EtcPalIpAddr *destinations, size_t destinations_size)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_change_priority</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga0ee9dd3d8114b4277cb73134d0ae2dc4</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, uint8_t new_priority)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_change_preview_flag</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga99d5fce087ce0b2b7d2caedf65bcce5e</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, bool new_preview_flag)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_change_synchronization_universe</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gac21fab7b1fe15c281e89303ce26d08fd</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, uint16_t new_sync_universe)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_send_now</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga6e7cea16d61ae355bc313452f0b47e48</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, uint8_t start_code, const uint8_t *buffer, size_t buflen)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_send_synchronization</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga0e07a954dab245c7d6f323d9bdeb2df3</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_update_levels</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga23dc503a9f5a46fce7e8448266760341</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, const uint8_t *new_levels, size_t new_levels_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_update_levels_and_pap</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga6a8835b680485d307f9d2d9c750517ab</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, const uint8_t *new_levels, size_t new_levels_size, const uint8_t *new_priorities, size_t new_priorities_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_update_levels_and_force_sync</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga56ea27101d95616ca53c5c602257311a</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, const uint8_t *new_levels, size_t new_levels_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_update_levels_and_pap_and_force_sync</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gabec53e52dbb1808a546832ef3446f009</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, const uint8_t *new_levels, size_t new_levels_size, const uint8_t *new_priorities, size_t new_priorities_size)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>sacn_source_process_manual</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga9d9b3e992e56375779651023cc92121d</anchor>
      <arglist>(sacn_source_tick_mode_t tick_mode)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_reset_networking</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>ga348c4a38439a1bf2623253bbc30120c5</anchor>
      <arglist>(const SacnNetintConfig *sys_netint_config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_reset_networking_per_universe</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gac47eb25e0017cf7d62120fb3fa072333</anchor>
      <arglist>(const SacnNetintConfig *sys_netint_config, const SacnSourceUniverseNetintList *per_universe_netint_lists, size_t num_per_universe_netint_lists)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>sacn_source_get_network_interfaces</name>
      <anchorfile>group__sacn__source.html</anchorfile>
      <anchor>gae8c8d77375ea48b26fc069c9068aec69</anchor>
      <arglist>(sacn_source_t handle, uint16_t universe, EtcPalMcastNetintId *netints, size_t netints_size)</arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>sacn_source_detector</name>
    <title>sACN Source Detector</title>
    <filename>group__sacn__source__detector.html</filename>
    <class kind="struct">SacnSourceDetectorCallbacks</class>
    <class kind="struct">SacnSourceDetectorConfig</class>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_DETECTOR_INFINITE</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>ga2a1bee1e4fe7a47ee053c8dbae05fbcb</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_DETECTOR_CONFIG_DEFAULT_INIT</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>ga78fbb7639c835e3b08091fc16e31d6fc</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnSourceDetectorSourceUpdatedCallback</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>ga30a59185e89668385ea425238cab8422</anchor>
      <arglist>)(sacn_remote_source_t handle, const EtcPalUuid *cid, const char *name, const uint16_t *sourced_universes, size_t num_sourced_universes, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnSourceDetectorSourceExpiredCallback</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>ga18cf47c111cd4d23ee36c8aae4c97158</anchor>
      <arglist>)(sacn_remote_source_t handle, const EtcPalUuid *cid, const char *name, void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>SacnSourceDetectorLimitExceededCallback</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>gadb2603e053f842705b654047e99c0096</anchor>
      <arglist>)(void *context)</arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnSourceDetectorCallbacks</type>
      <name>SacnSourceDetectorCallbacks</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>gaa539e91ba47bdb365998472761dcede2</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct SacnSourceDetectorConfig</type>
      <name>SacnSourceDetectorConfig</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>gacf1948692a93767d849b6df90f75f6a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_detector_config_init</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>gaaf2f5a453a68df1ed4dcc80e1a94a49a</anchor>
      <arglist>(SacnSourceDetectorConfig *config)</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_detector_create</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>gaa662eb1cc3bb937691dcabc56332952f</anchor>
      <arglist>(const SacnSourceDetectorConfig *config, const SacnNetintConfig *netint_config)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>sacn_source_detector_destroy</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>ga42bf1f29fa1a313343b672e33685ba1d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>etcpal_error_t</type>
      <name>sacn_source_detector_reset_networking</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>ga740a10eb54b67decf41794e298258a21</anchor>
      <arglist>(const SacnNetintConfig *sys_netint_config)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>sacn_source_detector_get_network_interfaces</name>
      <anchorfile>group__sacn__source__detector.html</anchorfile>
      <anchor>gae0c2cafc1d5d765f272981fc06f5fdc6</anchor>
      <arglist>(EtcPalMcastNetintId *netints, size_t netints_size)</arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>sacn_cpp_api</name>
    <title>sACN C++ Language APIs</title>
    <filename>group__sacn__cpp__api.html</filename>
    <subgroup>sacn_cpp_common</subgroup>
    <subgroup>sacn_dmx_merger_cpp</subgroup>
    <subgroup>sacn_merge_receiver_cpp</subgroup>
    <subgroup>sacn_receiver_cpp</subgroup>
    <subgroup>sacn_source_cpp</subgroup>
    <subgroup>sacn_source_detector_cpp</subgroup>
  </compound>
  <compound kind="group">
    <name>sacn_cpp_common</name>
    <title>Common Definitions</title>
    <filename>group__sacn__cpp__common.html</filename>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Init</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>gac1cf1cb5e698e8ad656481cc834925db</anchor>
      <arglist>(const EtcPalLogParams *log_params=nullptr, McastMode mcast_mode=McastMode::kEnabledOnAllInterfaces)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Init</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>ga462a83ab46a4fa6a2f642fcacabecc82</anchor>
      <arglist>(const EtcPalLogParams *log_params, std::vector&lt; SacnMcastInterface &gt; &amp;sys_netints)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Init</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>ga73781de46ab1321166a0dce24094c73d</anchor>
      <arglist>(std::vector&lt; SacnMcastInterface &gt; &amp;sys_netints)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Init</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>ga0dfea4ef6503c65fd4e13eb9813e6513</anchor>
      <arglist>(const etcpal::Logger &amp;logger, McastMode mcast_mode=McastMode::kEnabledOnAllInterfaces)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Error</type>
      <name>Init</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>ga414aefdf3a0364b6261374e61a37bd05</anchor>
      <arglist>(const etcpal::Logger &amp;logger, std::vector&lt; SacnMcastInterface &gt; &amp;sys_netints)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Deinit</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>ga7c27553e8de8ffb78e3627f51fe9eb25</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>RemoteSourceHandle</type>
      <name>GetRemoteSourceHandle</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>gaa353377fefbbdd6ae634d981944c5619</anchor>
      <arglist>(const etcpal::Uuid &amp;source_cid)</arglist>
    </member>
    <member kind="function">
      <type>etcpal::Expected&lt; etcpal::Uuid &gt;</type>
      <name>GetRemoteSourceCid</name>
      <anchorfile>group__sacn__cpp__common.html</anchorfile>
      <anchor>gab05243a382f6332a3d73dbb3b202bb95</anchor>
      <arglist>(RemoteSourceHandle source_handle)</arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>sacn_dmx_merger_cpp</name>
    <title>sACN DMX Merger API</title>
    <filename>group__sacn__dmx__merger__cpp.html</filename>
    <class kind="class">sacn::DmxMerger</class>
    <class kind="struct">sacn::DmxMerger::Settings</class>
  </compound>
  <compound kind="group">
    <name>sacn_merge_receiver_cpp</name>
    <title>sACN Merge Receiver API</title>
    <filename>group__sacn__merge__receiver__cpp.html</filename>
    <class kind="class">sacn::MergeReceiver</class>
    <class kind="class">sacn::MergeReceiver::NotifyHandler</class>
    <class kind="struct">sacn::MergeReceiver::Settings</class>
    <class kind="struct">sacn::MergeReceiver::NetintList</class>
    <class kind="struct">sacn::MergeReceiver::Source</class>
  </compound>
  <compound kind="group">
    <name>sacn_receiver_cpp</name>
    <title>sACN Receiver API</title>
    <filename>group__sacn__receiver__cpp.html</filename>
    <class kind="class">sacn::Receiver</class>
    <class kind="class">sacn::Receiver::NotifyHandler</class>
    <class kind="struct">sacn::Receiver::Settings</class>
    <class kind="struct">sacn::Receiver::NetintList</class>
  </compound>
  <compound kind="group">
    <name>sacn_source_cpp</name>
    <title>sACN Source API</title>
    <filename>group__sacn__source__cpp.html</filename>
    <class kind="class">sacn::Source</class>
    <class kind="struct">sacn::Source::Settings</class>
    <class kind="struct">sacn::Source::UniverseSettings</class>
    <class kind="struct">sacn::Source::UniverseNetintList</class>
  </compound>
  <compound kind="group">
    <name>sacn_source_detector_cpp</name>
    <title>sACN Source Detector API</title>
    <filename>group__sacn__source__detector__cpp.html</filename>
    <class kind="class">sacn::SourceDetector</class>
    <class kind="class">sacn::SourceDetector::NotifyHandler</class>
    <class kind="struct">sacn::SourceDetector::Settings</class>
  </compound>
  <compound kind="group">
    <name>sacnopts</name>
    <title>sACN Configuration Options</title>
    <filename>group__sacnopts.html</filename>
    <subgroup>sacnopts_global</subgroup>
    <subgroup>sacnopts_receiver</subgroup>
    <subgroup>sacnopts_send</subgroup>
    <subgroup>sacnopts_dmx_merger</subgroup>
    <subgroup>sacnopts_merge_receiver</subgroup>
    <subgroup>sacnopts_source_detector</subgroup>
  </compound>
  <compound kind="group">
    <name>sacnopts_global</name>
    <title>Global Options</title>
    <filename>group__sacnopts__global.html</filename>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DYNAMIC_MEM</name>
      <anchorfile>group__sacnopts__global.html</anchorfile>
      <anchor>gaf7b1d2fa482d1665683883f80b1f4d87</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_LOGGING_ENABLED</name>
      <anchorfile>group__sacnopts__global.html</anchorfile>
      <anchor>ga5b77f40b283fa0f754576a64ff6f3d4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_LOG_MSG_PREFIX</name>
      <anchorfile>group__sacnopts__global.html</anchorfile>
      <anchor>ga739093ac67975234540c307629ac8280</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_ASSERT_VERIFY</name>
      <anchorfile>group__sacnopts__global.html</anchorfile>
      <anchor>ga5a66b273b21d3c30e4531a590a480d2c</anchor>
      <arglist>(exp)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_ASSERT</name>
      <anchorfile>group__sacnopts__global.html</anchorfile>
      <anchor>gabd4f61db43336221d5947896ef0f2921</anchor>
      <arglist>(expr)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_ETC_PRIORITY_EXTENSION</name>
      <anchorfile>group__sacnopts__global.html</anchorfile>
      <anchor>ga59ee3abb2f4eb1554d7be219a9c8028b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_LOOPBACK</name>
      <anchorfile>group__sacnopts__global.html</anchorfile>
      <anchor>ga6ae6b1e22f87a5c2e49aefbc95a97256</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_MAX_NETINTS</name>
      <anchorfile>group__sacnopts__global.html</anchorfile>
      <anchor>ga2dd2edc6a9d13618baaf6d02b4d86e81</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>sacnopts_receiver</name>
    <title>sACN Receiver Options</title>
    <filename>group__sacnopts__receiver.html</filename>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_THREAD_PRIORITY</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga0262024b708fa546807d7b01485c7fcf</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_THREAD_STACK</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga3194913a252cb1da68e5bd919f6c1658</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_THREAD_NAME</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga0b3b952f010e515ba69fed3767e905f1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_READ_TIMEOUT_MS</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>gae7ae8c07912489f40fa77146cdc93d71</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_MAX_UNIVERSES</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga5d749b5a5b67d89a114aa2409ba6ff62</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga079b2f51919b9ac76e3b3330040bd20e</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_TOTAL_MAX_SOURCES</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga055b24b2c72073823f4c92ed32e7ddb0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_LIMIT_BIND</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga6bfc294d941da830fbb9bd562c45f638</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_SOCKET_PER_NIC</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>gac9daa024fb951f98cdf9c97ddc8244dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_MAX_SUBS_PER_SOCKET</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>gab20e9727556f9656a506bb85628f7df8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_ENABLE_SO_REUSEPORT</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>gaeed18ca4945f91d2108bdc32e587cf18</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_ENABLE_SO_RCVBUF</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga45b589b1c57b1812592ae77df67bd754</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_MAX_THREADS</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>ga064a427072df41e909b7d78cdb233c64</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_RECEIVER_MAX_FOOTPRINT</name>
      <anchorfile>group__sacnopts__receiver.html</anchorfile>
      <anchor>gaf0e0364237fe1e26c348fdd53bb5976f</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>sacnopts_send</name>
    <title>sACN Send Options</title>
    <filename>group__sacnopts__send.html</filename>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_THREAD_PRIORITY</name>
      <anchorfile>group__sacnopts__send.html</anchorfile>
      <anchor>gac69e2ffb7e6a156134373d687a7cbed7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_THREAD_STACK</name>
      <anchorfile>group__sacnopts__send.html</anchorfile>
      <anchor>ga197a7ace1de904752a545c799ea9e498</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_THREAD_NAME</name>
      <anchorfile>group__sacnopts__send.html</anchorfile>
      <anchor>ga0a2bddcd541aabbd0b4536d6dae4caa2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_SOCKET_SNDBUF_SIZE</name>
      <anchorfile>group__sacnopts__send.html</anchorfile>
      <anchor>gafb37b9b4569cb297e919655ad1f0ff22</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_MAX_SOURCES</name>
      <anchorfile>group__sacnopts__send.html</anchorfile>
      <anchor>gaa08afe863ab6922c173055d4239184c4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE</name>
      <anchorfile>group__sacnopts__send.html</anchorfile>
      <anchor>gaac75390f2990b299303bfbce89e5f7ef</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_MAX_UNICAST_DESTINATIONS_PER_UNIVERSE</name>
      <anchorfile>group__sacnopts__send.html</anchorfile>
      <anchor>ga53a3e7e498368c3311403912d7c01515</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>sacnopts_dmx_merger</name>
    <title>sACN DMX Merger Options</title>
    <filename>group__sacnopts__dmx__merger.html</filename>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_MAX_MERGERS</name>
      <anchorfile>group__sacnopts__dmx__merger.html</anchorfile>
      <anchor>ga7d15f54f43af8434b4967746f1282fb1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_MAX_MERGERS</name>
      <anchorfile>group__sacnopts__dmx__merger.html</anchorfile>
      <anchor>ga7d15f54f43af8434b4967746f1282fb1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER</name>
      <anchorfile>group__sacnopts__dmx__merger.html</anchorfile>
      <anchor>gaaeaa9701ec2b290ddfb3b31d8463dca4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_MAX_SLOTS</name>
      <anchorfile>group__sacnopts__dmx__merger.html</anchorfile>
      <anchor>ga01ed1a377484d83e568bd7cf053f392f</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_DISABLE_INTERNAL_PAP_BUFFER</name>
      <anchorfile>group__sacnopts__dmx__merger.html</anchorfile>
      <anchor>ga1291670a95116043ee7489006427aa5c</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_DMX_MERGER_DISABLE_INTERNAL_OWNER_BUFFER</name>
      <anchorfile>group__sacnopts__dmx__merger.html</anchorfile>
      <anchor>ga79ce98d0c9b30964fd17728dc8073a26</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>sacnopts_merge_receiver</name>
    <title>sACN Merge Receiver Options</title>
    <filename>group__sacnopts__merge__receiver.html</filename>
    <member kind="define">
      <type>#define</type>
      <name>SACN_MERGE_RECEIVER_ENABLE_IN_STATIC_MEMORY_MODE</name>
      <anchorfile>group__sacnopts__merge__receiver.html</anchorfile>
      <anchor>gaedd4882cf4778846d5e3e244fea9c916</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_MERGE_RECEIVER_ENABLE_SAMPLING_MERGER</name>
      <anchorfile>group__sacnopts__merge__receiver.html</anchorfile>
      <anchor>ga2e082ccf74be5a3b106b8623f5eaa65d</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>sacnopts_source_detector</name>
    <title>sACN Source Detector Options</title>
    <filename>group__sacnopts__source__detector.html</filename>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_DETECTOR_MAX_SOURCES</name>
      <anchorfile>group__sacnopts__source__detector.html</anchorfile>
      <anchor>ga872fc4bd419f3cc3d77e7502f0b1ed0e</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE</name>
      <anchorfile>group__sacnopts__source__detector.html</anchorfile>
      <anchor>ga24e186592053bd29fbffbfc92825268e</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="page">
    <name>additional_documentation</name>
    <title>Additional Documentation</title>
    <filename>additional_documentation.html</filename>
    <docanchor file="additional_documentation.html">md__tmp_tmpv3m2se2t_docs_pages_additional_documentation</docanchor>
  </compound>
  <compound kind="page">
    <name>building_and_integrating</name>
    <title>Building and Integrating the sACN Library Into Your Project</title>
    <filename>building_and_integrating.html</filename>
    <docanchor file="building_and_integrating.html">md__tmp_tmpv3m2se2t_docs_pages_building_and_integrating</docanchor>
  </compound>
  <compound kind="page">
    <name>configuring_lwip</name>
    <title>Configuring lwIP for compatibility with sACN</title>
    <filename>configuring_lwip.html</filename>
    <docanchor file="configuring_lwip.html">md__tmp_tmpv3m2se2t_docs_pages_configuring_lwip</docanchor>
  </compound>
  <compound kind="page">
    <name>getting_started</name>
    <title>Getting Started with sACN</title>
    <filename>getting_started.html</filename>
    <docanchor file="getting_started.html">md__tmp_tmpv3m2se2t_docs_pages_getting_started</docanchor>
  </compound>
  <compound kind="page">
    <name>global_init_and_destroy</name>
    <title>Global Initialization and Destruction</title>
    <filename>global_init_and_destroy.html</filename>
    <docanchor file="global_init_and_destroy.html">md__tmp_tmpv3m2se2t_docs_pages_global_init_and_destroy</docanchor>
  </compound>
  <compound kind="page">
    <name>per_address_priority</name>
    <title>Per Address Priority</title>
    <filename>per_address_priority.html</filename>
    <docanchor file="per_address_priority.html">md__tmp_tmpv3m2se2t_docs_pages_per_address_priority</docanchor>
  </compound>
  <compound kind="page">
    <name>source_loss_behavior</name>
    <title>Source Loss Behavior</title>
    <filename>source_loss_behavior.html</filename>
    <docanchor file="source_loss_behavior.html">md__tmp_tmpv3m2se2t_docs_pages_source_loss_behavior</docanchor>
  </compound>
  <compound kind="page">
    <name>using_dmx_merger</name>
    <title>Using the sACN DMX Merger API</title>
    <filename>using_dmx_merger.html</filename>
    <docanchor file="using_dmx_merger.html">md__tmp_tmpv3m2se2t_docs_pages_using_dmx_merger</docanchor>
  </compound>
  <compound kind="page">
    <name>using_merge_receiver</name>
    <title>Using the sACN Merge Receiver API</title>
    <filename>using_merge_receiver.html</filename>
    <docanchor file="using_merge_receiver.html">md__tmp_tmpv3m2se2t_docs_pages_using_merge_receiver</docanchor>
  </compound>
  <compound kind="page">
    <name>using_receiver</name>
    <title>Using the sACN Receiver API</title>
    <filename>using_receiver.html</filename>
    <docanchor file="using_receiver.html">md__tmp_tmpv3m2se2t_docs_pages_using_receiver</docanchor>
  </compound>
  <compound kind="page">
    <name>using_source</name>
    <title>Using the sACN Source API</title>
    <filename>using_source.html</filename>
    <docanchor file="using_source.html">md__tmp_tmpv3m2se2t_docs_pages_using_source</docanchor>
  </compound>
  <compound kind="page">
    <name>index</name>
    <title>Streaming ACN</title>
    <filename>index.html</filename>
  </compound>
</tagfile>
