import sys
from .datatypes import EmitDefinition
from . import content # pylint: disable=no-name-in-module
from . import network # pylint: disable=no-name-in-module

def create_enum_table(names, num):
	lines = []
	lines += ["enum", "{"]
	lines += [f"\t{names[0]}=0,"]
	for name in names[1:]:
		lines += [f"\t{name},"]
	lines += [f"\t{num}", "};"]
	return lines

def create_flags_table(names):
	lines = []
	lines += ["enum", "{"]
	i = 0
	for name in names:
		lines += [f"\t{name} = 1<<{int(i)},"]
		i += 1
	lines += ["};"]
	return lines

def EmitEnum(names, num):
	print("enum")
	print("{")
	print(f"\t{names[0]}=0,")
	for name in names[1:]:
		print(f"\t{name},")
	print(f"\t{num}")
	print("};")

def EmitFlags(names):
	print("enum")
	print("{")
	i = 0
	for name in names:
		print(f"\t{name} = 1<<{int(i)},")
		i += 1
	print("};")

def main():
	gen_network_header = "network_header" in sys.argv
	gen_network_source = "network_source" in sys.argv
	gen_client_content_header = "client_content_header" in sys.argv
	gen_client_content_source = "client_content_source" in sys.argv
	gen_server_content_header = "server_content_header" in sys.argv
	gen_server_content_source = "server_content_source" in sys.argv

	if gen_client_content_header:
		print("#ifndef CLIENT_CONTENT7_HEADER")
		print("#define CLIENT_CONTENT7_HEADER")

	if gen_server_content_header:
		print("#ifndef SERVER_CONTENT7_HEADER")
		print("#define SERVER_CONTENT7_HEADER")


	if gen_client_content_header or gen_server_content_header:
		# print some includes
		print('#include <engine/graphics.h>')
		print('#include "data_types.h"')
		print("namespace client_data7 {")

		# the container pointer
		print('extern CDataContainer *g_pData;')

		# enums
		EmitEnum([f"IMAGE_{i.name.value.upper()}" for i in content.container.images.items], "NUM_IMAGES")
		EmitEnum([f"ANIM_{i.name.value.upper()}" for i in content.container.animations.items], "NUM_ANIMS")
		EmitEnum([f"SPRITE_{i.name.value.upper()}" for i in content.container.sprites.items], "NUM_SPRITES")

	if gen_client_content_source or gen_server_content_source:
		if gen_client_content_source:
			print('#include "client_data7.h"')
		if gen_server_content_source:
			print('#include "server_data.h"')
		print("namespace client_data7 {")
		EmitDefinition(content.container, "datacontainer")
		print('CDataContainer *g_pData = &datacontainer;')
		print("}")

# NETWORK
	if gen_network_header:

		print("#ifndef GAME_GENERATED_PROTOCOL7_H")
		print("#define GAME_GENERATED_PROTOCOL7_H")
		print("class CUnpacker;")
		print("#include <engine/message.h>")
		print("namespace protocol7 {")
		print(network.RawHeader)

		for e in network.Enums:
			for l in create_enum_table([f"{e.name}_{v}" for v in e.values], f'NUM_{e.name}S'):
				print(l)
			print("")

		for e in network.Flags:
			for l in create_flags_table([f"{e.name}_{v}" for v in e.values]):
				print(l)
			print("")

		for l in create_enum_table(["NETOBJ_INVALID"]+[o.enum_name for o in network.Objects], "NUM_NETOBJTYPES"):
			print(l)
		print("")
		for l in create_enum_table(["NETMSG_INVALID"]+[o.enum_name for o in network.Messages], "NUM_NETMSGTYPES"):
			print(l)
		print("")

		print("""
	template<typename... Ts> struct make_void { typedef void type;};
	template<typename... Ts> using void_t = typename make_void<Ts...>::type;

	template<typename T, typename = void>
	struct is_sixup {
		constexpr static bool value = false;
	};

	template<typename T>
	struct is_sixup<T, void_t<typename T::is_sixup>> {
		constexpr static bool value = true;
	};
	""")

		for item in network.Objects + network.Messages:
			for line in item.emit_declaration():
				print(line)
			print("")

		EmitEnum([f"SOUND_{i.name.value.upper()}" for i in content.container.sounds.items], "NUM_SOUNDS")
		EmitEnum([f"WEAPON_{i.name.value.upper()}" for i in content.container.weapons.id.items], "NUM_WEAPONS")

		print("""

	class CNetObjHandler
	{
		const char *m_pMsgFailedOn;
		char m_aMsgData[1024];
		const char *m_pObjFailedOn;
		int m_NumObjFailures;
		bool CheckInt(const char *pErrorMsg, int Value, int Min, int Max);
		bool CheckFlag(const char *pErrorMsg, int Value, int Mask);

		static const char *ms_apObjNames[];
		static int ms_aObjSizes[];
		static const char *ms_apMsgNames[];

	public:
		CNetObjHandler();

		int ValidateObj(int Type, const void *pData, int Size);
		const char *GetObjName(int Type) const;
		int GetObjSize(int Type) const;
		const char *FailedObjOn() const;
		int NumObjFailures() const;

		const char *GetMsgName(int Type) const;
		void *SecureUnpackMsg(int Type, CUnpacker *pUnpacker);
		const char *FailedMsgOn() const;
	};

	""")

		print("}")
		print("#endif // GAME_GENERATED_PROTOCOL7_H")


	if gen_network_source:
		# create names
		lines = []

		lines += ['#include "protocol7.h"']
		lines += ['#include <base/system.h>']
		lines += ['#include <engine/shared/packer.h>']
		lines += ['#include <engine/shared/protocol.h>']

		lines += ['namespace protocol7 {']

		lines += ['CNetObjHandler::CNetObjHandler()']
		lines += ['{']
		lines += ['\tm_pMsgFailedOn = "";']
		lines += ['\tm_pObjFailedOn = "";']
		lines += ['\tm_NumObjFailures = 0;']
		lines += ['}']
		lines += ['']
		lines += ['const char *CNetObjHandler::FailedObjOn() const { return m_pObjFailedOn; }']
		lines += ['int CNetObjHandler::NumObjFailures() const { return m_NumObjFailures; }']
		lines += ['const char *CNetObjHandler::FailedMsgOn() const { return m_pMsgFailedOn; }']
		lines += ['']
		lines += ['']
		lines += ['']
		lines += ['']

		lines += ['static const int max_int = 0x7fffffff;']
		lines += ['']

		lines += ['bool CNetObjHandler::CheckInt(const char *pErrorMsg, int Value, int Min, int Max)']
		lines += ['{']
		lines += ['\tif(Value < Min || Value > Max) { m_pObjFailedOn = pErrorMsg; m_NumObjFailures++; return false; }']
		lines += ['\treturn true;']
		lines += ['}']
		lines += ['']

		lines += ['bool CNetObjHandler::CheckFlag(const char *pErrorMsg, int Value, int Mask)']
		lines += ['{']
		lines += ['\tif((Value&Mask) != Value) { m_pObjFailedOn = pErrorMsg; m_NumObjFailures++; return false; }']
		lines += ['\treturn true;']
		lines += ['}']
		lines += ['']

		lines += ["const char *CNetObjHandler::ms_apObjNames[] = {"]
		lines += ['\t"invalid",']
		lines += [f'\t"{o.name}",' for o in network.Objects]
		lines += ['\t""', "};", ""]

		lines += ["int CNetObjHandler::ms_aObjSizes[] = {"]
		lines += ['\t0,']
		lines += [f'\tsizeof({o.struct_name}),' for o in network.Objects]
		lines += ['\t0', "};", ""]


		lines += ['const char *CNetObjHandler::ms_apMsgNames[] = {']
		lines += ['\t"invalid",']
		for msg in network.Messages:
			lines += [f'\t"{msg.name}",']
		lines += ['\t""']
		lines += ['};']
		lines += ['']

		lines += ['const char *CNetObjHandler::GetObjName(int Type) const']
		lines += ['{']
		lines += ['\tif(Type < 0 || Type >= NUM_NETOBJTYPES) return "(out of range)";']
		lines += ['\treturn ms_apObjNames[Type];']
		lines += ['};']
		lines += ['']

		lines += ['int CNetObjHandler::GetObjSize(int Type) const']
		lines += ['{']
		lines += ['\tif(Type < 0 || Type >= NUM_NETOBJTYPES) return 0;']
		lines += ['\treturn ms_aObjSizes[Type];']
		lines += ['};']
		lines += ['']


		lines += ['const char *CNetObjHandler::GetMsgName(int Type) const']
		lines += ['{']
		lines += ['\tif(Type < 0 || Type >= NUM_NETMSGTYPES) return "(out of range)";']
		lines += ['\treturn ms_apMsgNames[Type];']
		lines += ['};']
		lines += ['']


		for l in lines:
			print(l)

		lines = []
		lines += ['int CNetObjHandler::ValidateObj(int Type, const void *pData, int Size)']
		lines += ['{']
		lines += ['\tswitch(Type)']
		lines += ['\t{']

		for item in network.Objects:
			for line in item.emit_validate(network.Objects):
				lines += ["\t" + line]
			lines += ['\t']
		lines += ['\t}']
		lines += ['\treturn -1;']
		lines += ['};']
		lines += ['']

		lines += ['void *CNetObjHandler::SecureUnpackMsg(int Type, CUnpacker *pUnpacker)']
		lines += ['{']
		lines += ['\tm_pMsgFailedOn = 0;']
		lines += ['\tm_pObjFailedOn = 0;']
		lines += ['\tswitch(Type)']
		lines += ['\t{']


		for item in network.Messages:
			for line in item.emit_unpack():
				lines += ["\t" + line]
			lines += ['\t']

		lines += ['\tdefault:']
		lines += ['\t\tm_pMsgFailedOn = "(type out of range)";']
		lines += ['\t\tbreak;']
		lines += ['\t}']
		lines += ['\t']
		lines += ['\tif(pUnpacker->Error())']
		lines += ['\t\tm_pMsgFailedOn = "(unpack error)";']
		lines += ['\t']
		lines += ['\tif(m_pMsgFailedOn || m_pObjFailedOn) {']
		lines += ['\t\tif(!m_pMsgFailedOn)']
		lines += ['\t\t\tm_pMsgFailedOn = "";']
		lines += ['\t\tif(!m_pObjFailedOn)']
		lines += ['\t\t\tm_pObjFailedOn = "";']
		lines += ['\t\treturn 0;']
		lines += ['\t}']
		lines += ['\tm_pMsgFailedOn = "";']
		lines += ['\tm_pObjFailedOn = "";']
		lines += ['\treturn m_aMsgData;']
		lines += ['};']
		lines += ['}']
		lines += ['']


		for l in lines:
			print(l)

	if gen_client_content_header or gen_server_content_header:
		print("}")
		print("#endif")

if __name__ == '__main__':
	main()
