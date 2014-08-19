#-------------------------------------------------------------------------------
# Copyright (c) 2014 Gael Honorez.
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the GNU Public License v3.0
# which accompanies this distribution, and is available at
# http://www.gnu.org/licenses/gpl.html
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#-------------------------------------------------------------------------------

import copy
from shaderManager.layers._layers import Layers
from shaderManager.assignations._assignationGroup import assignationGroup

class cacheAssignations(object):
    def __init__(self, parent=None):
        self.parent = parent

        self.mainAssignations = assignationGroup(self)
        self.mainAssignationsFromFile = assignationGroup(self, fromFile=True)

        self.layers = Layers(self)
        self.layersFromFile = Layers(self, fromFile=True)


    def writeLayer(self):
        fromfiledict = self.layersFromFile.getLayerDict()
        layerdict = self.layers.getLayerDict()

        cleanedDict = copy.deepcopy(layerdict)

        for layer in layerdict:
            if layer in fromfiledict:
                # check if the layer overrides were false or true in the orig file
                if fromfiledict[layer]["removeShaders"] == layerdict[layer]["removeShaders"] :
                    del cleanedDict[layer]["removeShaders"]

                if fromfiledict[layer]["removeDisplacements"] == layerdict[layer]["removeDisplacements"] :
                    del cleanedDict[layer]["removeDisplacements"]

                if fromfiledict[layer]["removeProperties"] == layerdict[layer]["removeProperties"]:
                    del cleanedDict[layer]["removeProperties"]
            else:

                if layerdict[layer]["removeShaders"] == False :
                    del cleanedDict[layer]["removeShaders"]

                if layerdict[layer]["removeDisplacements"] == False :
                    del cleanedDict[layer]["removeDisplacements"]

                if layerdict[layer]["removeProperties"] == False :
                    del cleanedDict[layer]["removeProperties"]

            if len(layerdict[layer]["properties"]) == 0:
                del cleanedDict[layer]["properties"]

            if len(layerdict[layer]["shaders"]) == 0:
                del cleanedDict[layer]["shaders"]


            if len(layerdict[layer]["displacements"]) == 0:
                del cleanedDict[layer]["displacements"]


            if len(cleanedDict[layer]) == 0:
                del cleanedDict[layer]

        self.parent.updateLayerOverrides(cleanedDict)

    def writeOverrides(self):
        self.parent.updateOverrides(self.mainAssignations.getOverrides())

    def addShaders(self, shaders, fromFile=False):
        if not fromFile:
            self.mainAssignations.addShaders(shaders)
        else:
            self.mainAssignationsFromFile.addShaders(shaders)

    def addOverrides(self, overrides, fromFile=False):
        if not fromFile:
            self.mainAssignations.addOverrides(overrides)
        else:
            self.mainAssignationsFromFile.addOverrides(overrides)

    def addDisplacements(self, displacements, fromFile=False):
        if not fromFile:
            self.mainAssignations.addDisplacements(displacements)
        else:
            self.mainAssignationsFromFile.addDisplacements(displacements)

    def addLayers(self, layers, fromFile=False):
        if not fromFile:
            self.layers.addLayers(layers, fromFile)
        else:
            self.layersFromFile.addLayers(layers, fromFile)


    def getShader(self, path, layer):
        shader = None
        if layer == None:
            shader = self.mainAssignations.getShaderFromPath(path)
            if not shader:
                shader = self.mainAssignationsFromFile.getShaderFromPath(path)
        else:
            shader = self.layers.getShaderFromPath(path, layer)
            if not shader:
                shader = self.layersFromFile.getShaderFromPath(path, layer)

        return shader

    def getAllShaders(self):
        return self.mainAssignations.getShaders().keys()  + self.layers.getShaders()

    def getAllDisplacements(self):
        return self.mainAssignations.getDisplacements().keys() + self.layers.getDisplacements()

    def getDisplace(self, path, layer):
        shader = None
        if layer == None:
            shader = self.mainAssignations.getDisplaceFromPath(path)
            if not shader:
                shader = self.mainAssignationsFromFile.getDisplaceFromPath(path)
        else:
            shader = self.layers.getDisplaceFromPath(path, layer)
            if not shader:
                shader = self.layersFromFile.getDisplaceFromPath(path, layer)

        return shader

    def getOverrides(self, path, layer):
        overrides = None
        if layer == None:
            overrides = self.mainAssignations.getOverridesFromPath(path)
            if not overrides:
                overrides = self.mainAssignationsFromFile.getOverridesFromPath(path)
        else:
            overrides = self.layers.getOverridesFromPath(path, layer)
            if not overrides:
                overrides = self.layersFromFile.getOverridesFromPath(path, layer)

        return overrides


    def getLayerOverrides(self, layer):
        layerOverrides = None
        layerOverrides = self.layers.getLayerOverrides(layer)
        if not layerOverrides:
            layerOverrides = self.layersFromFile.getLayerOverrides(layer)
        return layerOverrides


    def setRemovedShader(self, layer, state):
        self.layers.setRemovedShader(layer, state)

    def setRemovedDisplace(self, layer, state):
        self.layers.setRemovedDisplace(layer, state)

    def setRemovedProperties(self, layer, state):
        self.layers.setRemovedProperties(layer, state)


    def getPropertyState(self, layer, propName, curPath):
        if layer == None:
            if self.mainAssignationsFromFile.getOverrideValue(curPath, propName) != None:
                return 3
            if self.mainAssignations.getOverrideValue(curPath, propName) != None:
                return 2

        else:
            if self.layersFromFile.getOverrideValue(layer, curPath, propName) != None:
                return 3
            if self.layers.getOverrideValue(layer, curPath, propName) != None:
                return 1
            if self.mainAssignationsFromFile.getOverrideValue(curPath, propName) or self.mainAssignations.getOverrideValue(curPath, propName) != None:
                return 2

        return 0


    def updateOverride(self, propName, default, value, curPath, layer):
        if layer == None:
            # check assignation from file
            valueFromFile = self.mainAssignationsFromFile.getOverrideValue(curPath, propName)
            if valueFromFile != value:
                self.mainAssignations.updateOverride(propName, default, value, curPath)
            else:
                self.mainAssignations.removeOverride(curPath, propName)

            self.parent.updateOverrides(self.mainAssignations.getOverrides())
        else:
            valueFromMain = self.mainAssignations.getOverrideValue(curPath, propName)
            if not valueFromMain:
                valueFromMain= self.mainAssignationsFromFile.getOverrideValue(curPath, propName)

            valueFromFile = self.layersFromFile.getOverrideValue(layer, curPath, propName)

            if valueFromFile != value and valueFromMain != value:
                if valueFromFile != None or valueFromMain != None:
                    default = False

                self.layers.updateOverride(propName, default, value, curPath, layer)
            else:
                self.layers.removeOverride(layer, curPath, propName)

    def assignShader(self, layer, path, shader):
        if layer == None:
            self.mainAssignations.assignShader(path, shader)
            self.parent.updateShaders(self.mainAssignations.getShaders())
        else:
            self.layers.assignShader(layer, path, shader)

        self.parent.updateConnections()



    def assignDisplacement(self, layer, path, shader):
        if layer == None:
            self.mainAssignations.assignDisplacement(path, shader)
            self.parent.updateDisplacements(self.mainAssignations.getShaders())
        else:
            self.layers.assignDisplacement(layer, path, shader)

        self.parent.updateConnections()

