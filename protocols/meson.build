if enable_wayland
  wayland_protocols_generated_sources = []

  protocols = {
    'wlr-screencopy-unstable-v1': wlr_protocols_pkgdatadir / 'unstable' / 'wlr-screencopy-unstable-v1.xml',
    'ext-foreign-toplevel-list-v1': wl_protocols_pkgdatadir / 'staging' / 'ext-foreign-toplevel-list' / 'ext-foreign-toplevel-list-v1.xml',
    'ext-image-capture-source-v1': wl_protocols_pkgdatadir / 'staging' / 'ext-image-capture-source' / 'ext-image-capture-source-v1.xml',
    'ext-image-copy-source-v1': wl_protocols_pkgdatadir / 'staging' / 'ext-image-copy-capture' / 'ext-image-copy-capture-v1.xml',
  }

  foreach name, file : protocols
    client_header = custom_target(
      '@0@-client-header'.format(name),
      input: file,
      output: '@0@-client.h'.format(name),
      command: [wayland_scanner_bin, 'client-header', '@INPUT@', '@OUTPUT@'],
    )

    private_code = custom_target(
      '@0@-private-code'.format(name),
      input: file,
      output: '@0@.c'.format(name),
      command: [wayland_scanner_bin, 'private-code', '@INPUT@', '@OUTPUT@'],
    )

    wayland_protocols_generated_sources += [client_header, private_code]
  endforeach
endif
