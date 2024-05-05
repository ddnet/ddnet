def only(x):
	if len(x) != 1:
		raise ValueError
	return list(x)[0]

GlobalIdCounter = 0
def GetId():
	global GlobalIdCounter
	GlobalIdCounter += 1
	return GlobalIdCounter
def GetUID():
	return f"x{int(GetId())}"

def FixCasing(Str):
	NewStr = ""
	NextUpperCase = True
	for c in Str:
		if NextUpperCase:
			NextUpperCase = False
			NewStr += c.upper()
		else:
			if c == "_":
				NextUpperCase = True
			else:
				NewStr += c.lower()
	return NewStr

def FormatName(typ, name):
	if "*" in typ:
		return "m_p" + FixCasing(name)
	if "[]" in typ:
		return "m_a" + FixCasing(name)
	return "m_" + FixCasing(name)

class BaseType:
	def __init__(self, type_name):
		self._type_name = type_name
		self._target_name = "INVALID"
		self._id = GetId() # this is used to remember what order the members have in structures etc

	def Identifier(self):
		return "x"+str(self._id)
	def TargetName(self):
		return self._target_name
	def TypeName(self):
		return self._type_name
	def Id(self):
		return self._id

	def EmitDeclaration(self, name):
		return [f"{self.TypeName()} {FormatName(self.TypeName(), name)};"]
	def EmitPreDefinition(self, target_name):
		self._target_name = target_name
		return []
	def EmitDefinition(self, _name):
		return []

class MemberType:
	def __init__(self, name, var):
		self.name = name
		self.var = var

class Struct(BaseType):
	def __init__(self, type_name):
		BaseType.__init__(self, type_name)
	def Members(self):
		def sorter(a):
			return a.var.Id()
		m = []
		for name, value in self.__dict__.items():
			if name[0] == "_":
				continue
			m += [MemberType(name, value)]
		m.sort(key = sorter)
		return m

	def EmitTypeDeclaration(self, _name):
		lines = []
		lines += ["struct " + self.TypeName()]
		lines += ["{"]
		for member in self.Members():
			lines += ["\t"+l for l in member.var.EmitDeclaration(member.name)]
		lines += ["};"]
		return lines

	def EmitPreDefinition(self, target_name):
		BaseType.EmitPreDefinition(self, target_name)
		lines = []
		for member in self.Members():
			lines += member.var.EmitPreDefinition(target_name+"."+member.name)
		return lines
	def EmitDefinition(self, _name):
		lines = [f"/* {self.TargetName()} */ {{"]
		for member in self.Members():
			lines += ["\t" + " ".join(member.var.EmitDefinition("")) + ","]
		lines += ["}"]
		return lines

class Array(BaseType):
	def __init__(self, typ):
		BaseType.__init__(self, typ.TypeName())
		self.type = typ
		self.items = []
	def Add(self, instance):
		if instance.TypeName() != self.type.TypeName():
			raise ValueError("bah")
		self.items += [instance]
	def EmitDeclaration(self, name):
		return [f"int m_Num{FixCasing(name)};",
			f"{self.TypeName()} *{FormatName('[]', name)};"]
	def EmitPreDefinition(self, target_name):
		BaseType.EmitPreDefinition(self, target_name)

		lines = []
		i = 0
		for item in self.items:
			lines += item.EmitPreDefinition(f"{self.Identifier()}[{int(i)}]")
			i += 1

		if self.items:
			lines += [f"static {self.TypeName()} {self.Identifier()}[] = {{"]
			for item in self.items:
				itemlines = item.EmitDefinition("")
				lines += ["\t" + " ".join(itemlines).replace("\t", " ") + ","]
			lines += ["};"]
		else:
			lines += [f"static {self.TypeName()} *{self.Identifier()} = 0;"]

		return lines
	def EmitDefinition(self, _name):
		return [str(len(self.items))+","+self.Identifier()]

# Basic Types

class Int(BaseType):
	def __init__(self, value):
		BaseType.__init__(self, "int")
		self.value = value
	def Set(self, value):
		self.value = value
	def EmitDefinition(self, _name):
		return [f"{int(self.value)}"]
		#return ["%d /* %s */"%(self.value, self._target_name)]

class Float(BaseType):
	def __init__(self, value):
		BaseType.__init__(self, "float")
		self.value = value
	def Set(self, value):
		self.value = value
	def EmitDefinition(self, _name):
		return [f"{self.value:f}f"]
		#return ["%d /* %s */"%(self.value, self._target_name)]

class String(BaseType):
	def __init__(self, value):
		BaseType.__init__(self, "const char*")
		self.value = value
	def Set(self, value):
		self.value = value
	def EmitDefinition(self, _name):
		return ['"'+self.value+'"']

class Pointer(BaseType):
	def __init__(self, typ, target):
		BaseType.__init__(self, f"{typ().TypeName()}*")
		self.target = target
	def Set(self, target):
		self.target = target
	def EmitDefinition(self, _name):
		return ["&"+self.target.TargetName()]

class TextureHandle(BaseType):
	def __init__(self):
		BaseType.__init__(self, "IGraphics::CTextureHandle")
	def EmitDefinition(self, _name):
		return ["IGraphics::CTextureHandle()"]

# helper functions

def EmitTypeDeclaration(root):
	for l in root().EmitTypeDeclaration(""):
		print(l)

def EmitDefinition(root, name):
	for l in root.EmitPreDefinition(name):
		print(l)
	print(f"{root.TypeName()} {name} = ")
	for l in root.EmitDefinition(name):
		print(l)
	print(";")

# Network stuff after this

class Object:
	pass

class Enum:
	def __init__(self, name, values, start = 0):
		self.name = name
		self.values = values
		self.start = start

class Flags:
	def __init__(self, name, values):
		self.name = name
		self.values = values

class NetObject:
	def __init__(self, name, variables, ex=None, validate_size=True):
		l = name.split(":")
		self.name = l[0]
		self.base = None
		self.base_struct_name = None
		if len(l) > 1:
			self.base = l[1]
			self.base_struct_name = f"CNetObj_{self.base}"
		self.struct_name = f"CNetObj_{self.name}"
		self.enum_name = f"NETOBJTYPE_{self.name.upper()}"
		self.variables = variables
		self.ex = ex
		self.validate_size = validate_size

	def emit_declaration(self):
		lines = []
		if self.base is not None:
			lines += [f"struct {self.struct_name} : public {self.base_struct_name}", "{"]
		else:
			lines += [f"struct {self.struct_name}", "{"]
		lines += [f"\tstatic constexpr int ms_MsgId = {self.enum_name};"]
		for v in self.variables:
			lines += ["\t"+line for line in v.emit_declaration()]
		lines += ["};"]
		return lines

	def emit_uncompressed_unpack_and_validate(self, objects):
		lines = []
		lines += [f"case {self.enum_name}:"]
		lines += ["{"]
		lines += [f"\t{self.struct_name} *pData = ({self.struct_name} *)m_aUnpackedData;"]
		unpack_lines = []

		variables = self.variables
		next_base_name = self.base
		while next_base_name is not None:
			base_item = only([i for i in objects if i.name == next_base_name])
			variables = base_item.variables + variables
			next_base_name = base_item.base
		for v in variables:
			if not self.validate_size and v.default is None:
				raise ValueError(f"{v.name} in {self.name} has no default value. Member variables that do not have a default value cannot be used in a structure whose size is not validated.")
			unpack_lines += ["\t"+line for line in v.emit_uncompressed_unpack_obj()]
		for v in variables:
			unpack_lines += ["\t"+line for line in v.emit_validate_obj()]

		if len(unpack_lines) > 0:
			lines += unpack_lines
		else:
			lines += ["\t(void)pData;"]
		lines += ["} break;"]
		return lines

class NetEvent(NetObject):
	def __init__(self, name, variables, ex=None):
		NetObject.__init__(self, name, variables, ex=ex)
		if self.base is not None:
			self.base_struct_name = f"CNetEvent_{self.base}"
		self.struct_name = f"CNetEvent_{self.name}"
		self.enum_name = f"NETEVENTTYPE_{self.name.upper()}"

class NetMessage(NetObject):
	def __init__(self, name, variables, ex=None, teehistorian=True):
		NetObject.__init__(self, name, variables, ex=ex)
		if self.base is not None:
			self.base_struct_name = f"CNetMsg_{self.base}"
		self.struct_name = f"CNetMsg_{self.name}"
		self.enum_name = f"NETMSGTYPE_{self.name.upper()}"
		self.teehistorian = teehistorian

	def emit_unpack_msg(self):
		lines = []
		lines += [f"case {self.enum_name}:"]
		lines += ["{"]
		lines += [f"\t{self.struct_name} *pData = ({self.struct_name} *)m_aUnpackedData;"]

		unpack_lines = []
		for v in self.variables:
			unpack_lines += ["\t"+line for line in v.emit_unpack_msg()]
		for v in self.variables:
			unpack_lines += ["\t"+line for line in v.emit_unpack_msg_check()]

		if len(unpack_lines) > 0:
			lines += unpack_lines
		else:
			lines += ["\t(void)pData;"]
		lines += ["} break;"]
		return lines

	def emit_declaration(self):
		extra = []
		extra += ["\t"]
		extra += ["\tbool Pack(CMsgPacker *pPacker) const"]
		extra += ["\t{"]
		#extra += ["\t\tmsg_pack_start(%s, flags);"%self.enum_name]
		for v in self.variables:
			extra += ["\t\t"+line for line in v.emit_pack()]
		extra += ["\t\treturn pPacker->Error() != 0;"]
		extra += ["\t}"]

		lines = NetObject.emit_declaration(self)
		lines = lines[:-1] + extra + lines[-1:]
		return lines

class NetObjectEx(NetObject):
	def __init__(self, name, ex, variables, validate_size=True):
		NetObject.__init__(self, name, variables, ex=ex, validate_size=validate_size)

class NetEventEx(NetEvent):
	def __init__(self, name, ex, variables):
		NetEvent.__init__(self, name, variables, ex=ex)

class NetMessageEx(NetMessage):
	def __init__(self, name, ex, variables, teehistorian=True):
		NetMessage.__init__(self, name, variables, ex=ex)


class NetVariable:
	def __init__(self, name, default=None):
		self.name = name
		self.default = None if default is None else str(default)
	def emit_declaration(self):
		return []
	def emit_validate_obj(self):
		return []
	def emit_uncompressed_unpack_obj(self):
		return []
	def emit_pack(self):
		return []
	def emit_unpack_msg(self):
		return []
	def emit_unpack_msg_check(self):
		return []

class NetString(NetVariable):
	def emit_declaration(self):
		return [f"const char *{self.name};"]
	def emit_uncompressed_unpack_obj(self):
		return self.emit_unpack_msg()
	def emit_unpack_msg(self):
		return [f"pData->{self.name} = pUnpacker->GetString();"]
	def emit_pack(self):
		return [f"pPacker->AddString({self.name}, -1);"]

class NetStringHalfStrict(NetVariable):
	def emit_declaration(self):
		return [f"const char *{self.name};"]
	def emit_uncompressed_unpack_obj(self):
		return self.emit_unpack_msg()
	def emit_unpack_msg(self):
		return [f"pData->{self.name} = pUnpacker->GetString(CUnpacker::SANITIZE_CC);"]
	def emit_pack(self):
		return [f"pPacker->AddString({self.name}, -1);"]

class NetStringStrict(NetVariable):
	def emit_declaration(self):
		return [f"const char *{self.name};"]
	def emit_uncompressed_unpack_obj(self):
		return self.emit_unpack_msg()
	def emit_unpack_msg(self):
		return [f"pData->{self.name} = pUnpacker->GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES);"]
	def emit_pack(self):
		return [f"pPacker->AddString({self.name}, -1);"]

class NetIntAny(NetVariable):
	def emit_declaration(self):
		return [f"int {self.name};"]
	def emit_uncompressed_unpack_obj(self):
		if self.default is None:
			return [f"pData->{self.name} = pUnpacker->GetUncompressedInt();"]
		return [f"pData->{self.name} = pUnpacker->GetUncompressedIntOrDefault({self.default});"]
	def emit_unpack_msg(self):
		if self.default is None:
			return [f"pData->{self.name} = pUnpacker->GetInt();"]
		return [f"pData->{self.name} = pUnpacker->GetIntOrDefault({self.default});"]
	def emit_pack(self):
		return [f"pPacker->AddInt({self.name});"]

class NetIntRange(NetIntAny):
	def __init__(self, name, min_val, max_val, default=None):
		NetIntAny.__init__(self,name,default=default)
		self.min = str(min_val)
		self.max = str(max_val)
	def emit_validate_obj(self):
		return [f"pData->{self.name} = ClampInt(\"{self.name}\", pData->{self.name}, {self.min}, {self.max});"]
	def emit_unpack_msg_check(self):
		return [f"if(pData->{self.name} < {self.min} || pData->{self.name} > {self.max}) {{ m_pMsgFailedOn = \"{self.name}\"; break; }}"]

class NetBool(NetIntRange):
	def __init__(self, name, default=None):
		default = None if default is None else int(default)
		NetIntRange.__init__(self,name,0,1,default=default)

class NetTick(NetIntAny):
	def __init__(self, name, default=None):
		NetIntAny.__init__(self,name,default=default)

class NetArray(NetVariable):
	def __init__(self, var, size):
		NetVariable.__init__(self,var.name,var.default)
		self.base_name = var.name
		self.var = var
		self.size = size
		self.name = self.base_name + f"[{int(self.size)}]"
	def emit_declaration(self):
		self.var.name = self.name
		return self.var.emit_declaration()
	def emit_uncompressed_unpack_obj(self):
		lines = []
		for i in range(self.size):
			self.var.name = self.base_name + f"[{int(i)}]"
			lines += self.var.emit_uncompressed_unpack_obj()
		return lines
	def emit_validate_obj(self):
		lines = []
		for i in range(self.size):
			self.var.name = self.base_name + f"[{int(i)}]"
			lines += self.var.emit_validate_obj()
		return lines
	def emit_unpack_msg(self):
		lines = []
		for i in range(self.size):
			self.var.name = self.base_name + f"[{int(i)}]"
			lines += self.var.emit_unpack_msg()
		return lines
	def emit_pack(self):
		lines = []
		for i in range(self.size):
			self.var.name = self.base_name + f"[{int(i)}]"
			lines += self.var.emit_pack()
		return lines
	def emit_unpack_msg_check(self):
		lines = []
		for i in range(self.size):
			self.var.name = self.base_name + f"[{int(i)}]"
			lines += self.var.emit_unpack_msg_check()
		return lines
