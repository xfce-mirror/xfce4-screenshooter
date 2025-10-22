libscreenshooter_sources = [
  'libscreenshooter.h',
  'screenshooter-actions.c',
  'screenshooter-actions.h',
  'screenshooter-capture.c',
  'screenshooter-capture.h',
  'screenshooter-custom-actions.c',
  'screenshooter-custom-actions.h',
  'screenshooter-format.c',
  'screenshooter-format.h',
  'screenshooter-dialogs.c',
  'screenshooter-dialogs.h',
  'screenshooter-global.h',
  'screenshooter-select.c',
  'screenshooter-select.h',
  'screenshooter-utils.c',
  'screenshooter-utils.h',
]

libscreenshooter_sources += gnome.genmarshal(
  'screenshooter-marshal',
  sources: 'screenshooter-marshal.list',
  prefix: '_screenshooter_marshal',
  internal: true,
  install_header: false,
)

if enable_x11
  libscreenshooter_sources += [
    'screenshooter-capture-x11.c',
    'screenshooter-capture-x11.h',
    'screenshooter-select-x11.c',
    'screenshooter-select-x11.h',
    'screenshooter-utils-x11.c',
    'screenshooter-utils-x11.h',
  ]
endif

if enable_wayland
  libscreenshooter_sources += wayland_protocols_generated_sources
  libscreenshooter_sources += [
    'screenshooter-capture-wayland.c',
    'screenshooter-capture-wayland.h',
    'screenshooter-select-wayland.c',
    'screenshooter-select-wayland.h',
  ]
endif

libscreenshooter = static_library(
  'libscreenshooter',
  libscreenshooter_sources,
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gtk,
    gtk_layer_shell,
    exo,
    libxfce4ui,
    libxfce4util,
    xfconf,
    x11_deps,
    wayland_deps,
    xfixes,
  ],
  install: false,
)
