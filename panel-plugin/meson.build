plugin_sources = [
  'screenshooter-plugin.c'
]

plugin_install_subdir = 'xfce4' / 'panel' / 'plugins'

shared_module(
  'screenshooterplugin',
  plugin_sources,
  gnu_symbol_visibility: 'hidden',
  include_directories: [
    include_directories('..', '..' / 'lib'),
  ],
  dependencies: [
    gtk,
    glib,
    libxfce4util,
    libxfce4ui,
    libxfce4panel,
    xfconf,
    x11_deps,
    wayland_deps,
    xfixes,
  ],
  link_with: [
    libscreenshooter,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / plugin_install_subdir,
)

i18n.merge_file(
    input: 'screenshooter.desktop.in',
    output: 'screenshooter.desktop',
    po_dir: '..' / 'po',
    type: 'desktop',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / plugin_install_subdir,
)
