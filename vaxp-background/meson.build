project('vaxp-background', 'c',
  version : '0.1.0',
  default_options : ['warning_level=2', 'c_std=gnu11'])

cc = meson.get_compiler('c')

gtk_dep = dependency('gtk+-3.0')
gio_dep = dependency('gio-2.0')
layer_shell_dep = dependency('gtk-layer-shell-0')
m_dep = cc.find_library('m', required : false)

deps = [gtk_dep, gio_dep, layer_shell_dep, m_dep]

inc = include_directories('include')

# vaxp-background service
bg_srcs = [
  'src/main.c',
  'src/wallpaper.c',
  'src/desktop_config.c'
]

executable('vaxp-background',
  bg_srcs,
  include_directories : inc,
  dependencies : deps,
  install : true)

# vaxp-setbg tool
setbg_srcs = [
  'src/vaxp-setbg.c'
]

setbg_deps = [gtk_dep, gio_dep]

executable('vaxp-setbg',
  setbg_srcs,
  include_directories : inc,
  dependencies : setbg_deps,
  install : true)
