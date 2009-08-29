#!/usr/bin/python

#
# @brief extract code structure about behaviour from .h and .c files
# @license Public domain
#

import os, os.path, sys

# path where exists ufo binary
UFOAI_ROOT = os.path.realpath(sys.path[0] + '/../../..')

dir = UFOAI_ROOT + '/src/client/menu/node'

class Element:
	def __init__(self, name, type):
		self.doc = []
		self.name = name
		self.type = type
		pass

	def addDoc(self, comments):
		self.doc = comments

class NodeBehaviour:

	def __init__(self):
		self.name = ""
		self.extends = "abstractnode"
		self.properties = {}
		self.methods = {}
		self.confuncs = {}
		self.doc = ""
		pass

	def init(self, dic):
		self.name = dic['name']
		if (self.name == "abstractnode"):
			self.extends = None
		elif 'extends' in dic:
			self.extends = dic['extends']
		if self.name == "":
			self.name = "null"

	def addProperty(self, element):
		if element.type == "V_UI_NODEMETHOD":
			self.methods[element.name] = element
		else:
			self.properties[element.name] = element

	def initProperties(self, dic):
		self.properties = dic

	def initMethods(self, dic):
		self.methods = dic

# @brief Extract node behaviour from source code
class ExtractNodeBehaviour:

	def __init__(self):
		self.behaviours = {}
		self.extractData()

	def getBehaviours(self):
		return self.behaviours

	# @brief Extract each properties into a text according to the structure name
	def extractProperties(self, node, filedata, structName):
		signature = "const value_t %s[]" % structName
		properties = filedata.split(signature)
		properties = properties[1]

		i = properties.find('{')
		j = properties.find('};')
		properties = properties[i+1:j]

		result = {}
		comments = []
		for line in properties.splitlines():
			line = line.strip()

			if line.startswith("/*"):
				line = line.replace('/**', '').replace('/*', '').replace('*/', '')
				line = line.strip()
				comments.append(line)
				continue

			if line.startswith("*/"):
				continue

			if line.startswith("*"):
				line = line.replace('*', '', 1).replace('*/', '')
				line = line.strip()
				if line.startswith("@"):
					comments.append(line)
				else:
					comments[len(comments)-1] += ' ' + line
				continue

			i = line.find('{')
			if i == -1:
				continue

			line = line[i+1:]
			e = line.split(',')

			name = e[0].strip()
			if name[0] == '"':
				name = name[1:len(name)-1]

			if name == 'NULL':
				continue

			type = e[1].strip()

			element = Element(name, type)
			element.addDoc(comments)
			node.addProperty(element)
			comments = []

	# @brief Extract each body of registration function into a text
	def extractRegistrationFunctions(self, filedata):
		result = []

		register = filedata.split("\nvoid MN_Register")
		register.pop(0)

		for body in register:
			body = body.split('{')[1]
			body = body.split('}')[0]
			result.append(body)

		return result

	def extractData(self):
		# all nodes
		for f in os.listdir(dir):
			if ".c" not in f:
				continue

			file = open(dir + '/' + f, "rt")
			data = file.read()
			file.close()

			for code in self.extractRegistrationFunctions(data):
				lines = code.split('\n')
				dic = {}
				for l in lines:
					l = l.replace("behaviour->", "")
					l = l.replace(";", "")
					e = l.split('=')
					if len(e) != 2:
						continue
					v = e[1].strip()
					if v[0] == '"':
						v = v[1:len(v)-1]
					dic[e[0].strip()] = v

				node = NodeBehaviour()
				node.init(dic)
				if 'properties' in dic:
					props = self.extractProperties(node, data, dic['properties'])

				self.behaviours[node.name] = node
