from conans import ConanFile, CMake, tools


class AbctoaConan(ConanFile):
    name = "abcToA"
    version = "latest"
    settings = "os", "compiler", "build_type", "arch"
    options = { "maya_version": [2015, 2017] }
    requires = ( "Alembic/1.7.0@digic/stable"
               , "Arnold/4.2.16.3@digic/stable"
               , "Boost/1.61@digic/stable"
               , "IlmBase/2.2.0@digic/stable"
               , "jsoncpp/1.7.7@digic/stable"
               , "pystring/1.1.3@digic/stable"
               )
    default_options = ( "maya_version=2017"
                      , "boost:shared=True"
                      , "zlib:shared=False"
                      , "IlmBase:shared=False"
                      , "jsoncpp:shared=False"
                      , "pystring:shared=False"
                      , "Alembic:shared=False"
                      , "HDF5:shared=False"
                      )
    generators = "cmake"

    def requirements(self):
         self.requires("Maya/{}@digic/stable".format(self.options.maya_version))
