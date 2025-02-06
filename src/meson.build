screenshooter_sources = [
  'main.c'
]

executable(
  'xfce4-screenshooter',
  screenshooter_sources,
  sources: xfce_revision_h,
  include_directories: [
    include_directories('..', '..' / 'lib'),
  ],
  dependencies: [
    gtk,
    glib,
    libxfce4util,
    libxfce4ui,
    xfconf,
    x11_deps,
    wayland_deps
  ],
  link_with: [
    libscreenshooter
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

i18n.merge_file(
    input: 'xfce4-screenshooter.desktop.in',
    output: 'xfce4-screenshooter.desktop',
    po_dir: '..' / 'po',
    type: 'desktop',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'applications',
)

i18n.merge_file(
    input: 'xfce4-screenshooter.appdata.xml.in',
    output: 'xfce4-screenshooter.appdata.xml',
    po_dir: '..' / 'po',
    type: 'xml',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'metainfo',
)

help2man = find_program('help2man', required: false)
if help2man.found()
  custom_target(
    'xfce4-screenshooter.1',
    output: 'xfce4-screenshooter.1',
    command: [
      help2man, ['--no-info'], '--output=@OUTPUT@', 'xfce4-screenshooter'
    ],
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'man' / 'man1'
  )
endif
