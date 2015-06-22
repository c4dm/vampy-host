from distutils.core import setup, Extension

sdkdir = 'vamp-plugin-sdk/src/vamp-hostsdk/'
vpydir = 'native/'

sdkfiles = [ 'Files', 'PluginBufferingAdapter', 'PluginChannelAdapter',
             'PluginHostAdapter', 'PluginInputDomainAdapter', 'PluginLoader',
             'PluginSummarisingAdapter', 'PluginWrapper', 'RealTime' ]
vpyfiles = [ 'PyPluginObject', 'PyRealTime', 'VectorConversion', 'vampyhost' ]

srcfiles = [ sdkdir + f + '.cpp' for f in sdkfiles ] + [ vpydir + f + '.cpp' for f in vpyfiles ]

vampyhost = Extension('vampyhost',
                      sources = srcfiles,
                      include_dirs = [ 'vamp-plugin-sdk' ])

setup (name = 'vamp',
       version = '1.0',
       description = 'This module allows Python code to load and use Vamp plugins for audio feature analysis.',
       requires = [ 'numpy' ],
       ext_modules = [ vampyhost ])
