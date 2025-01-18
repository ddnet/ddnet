import argparse
import content
import network

from datatypes import EmitDefinition, EmitTypeDeclaration

def create_enum_table(names, num, start = 0):
	lines = []
	lines += ["enum", "{"]
	if len(names) > 0 and start != 0:
		lines += [f"\t{names[0]} = {start},"]
		names = names[1:]
	for name in names:
		lines += [f"\t{name},"]
	lines += [f"\t{num}", "};"]
	return lines


def create_flags_table(names):
	lines = []
	lines += ["enum", "{"]
	for i, name in enumerate(names):
		lines += [f"\t{name} = 1<<{int(i)},"]
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
	for i, name in enumerate(names):
		print(f"\t{name} = 1<<{int(i)},")
	print("};")


def gen_network_header():
	print("#ifndef GAME_GENERATED_PROTOCOL_H")
	print("#define GAME_GENERATED_PROTOCOL_H")
	print("class CUnpacker;")
	print("#include <engine/message.h>")
	print(network.RawHeader)

	for e in network.Enums:
		for line in create_enum_table([f"{e.name}_{v}" for v in e.values], f'NUM_{e.name}S', e.start): # pylint: disable=no-member
			print(line)
		print("")

	for e in network.Flags:
		for line in create_flags_table([f"{e.name}_{v}" for v in e.values]):
			print(line)
		print("")

	non_extended = [o for o in network.Objects if o.ex is None]
	extended = [o for o in network.Objects if o.ex is not None]
	for line in create_enum_table(["NETOBJTYPE_EX"]+[o.enum_name for o in non_extended], "NUM_NETOBJTYPES"):
		print(line)
	for line in create_enum_table(["__NETOBJTYPE_UUID_HELPER=OFFSET_GAME_UUID-1"]+[o.enum_name for o in extended], "OFFSET_NETMSGTYPE_UUID"):
		print(line)
	print("")

	non_extended = [o for o in network.Messages if o.ex is None]
	extended = [o for o in network.Messages if o.ex is not None]
	for line in create_enum_table(["NETMSGTYPE_EX"]+[o.enum_name for o in non_extended], "NUM_NETMSGTYPES"):
		print(line)
	print("")
	for line in create_enum_table(["__NETMSGTYPE_UUID_HELPER=OFFSET_NETMSGTYPE_UUID-1"]+[o.enum_name for o in extended], "OFFSET_MAPITEMTYPE_UUID"):
		print(line)
	print("")

	for item in network.Objects + network.Messages:
		for line in item.emit_declaration():
			print(line)
		print("")

	EmitEnum([f"SOUND_{i.name.value.upper()}" for i in content.container.sounds.items], "NUM_SOUNDS")
	EmitEnum([f"WEAPON_{i.name.value.upper()}" for i in content.container.weapons.id.items], "NUM_WEAPONS")

	print("""
enum
{
	WEAPON_GAME = -3, // team switching etc
	WEAPON_SELF = -2, // console kill command
	WEAPON_WORLD = -1, // death tiles etc
};

class CNetObjHandler
{
	const char *m_pMsgFailedOn;
	const char *m_pObjFailedOn;
	const char *m_pObjCorrectedOn;
	char m_aUnpackedData[1024 * 2];
	int m_NumObjCorrections;
	int ClampInt(const char *pErrorMsg, int Value, int Min, int Max);

	static const char *ms_apObjNames[];
	static const char *ms_apExObjNames[];
	static int ms_aObjSizes[];
	static int ms_aUnpackedObjSizes[];
	static int ms_aUnpackedExObjSizes[];
	static const char *ms_apMsgNames[];
	static const char *ms_apExMsgNames[];

public:
	CNetObjHandler();

	void *SecureUnpackObj(int Type, CUnpacker *pUnpacker);
	const char *GetObjName(int Type) const;
	int GetObjSize(int Type) const;
	int GetUnpackedObjSize(int Type) const;
	int NumObjCorrections() const;
	const char *CorrectedObjOn() const;
	const char *FailedObjOn() const;

	const char *GetMsgName(int Type) const;
	void *SecureUnpackMsg(int Type, CUnpacker *pUnpacker);
	bool TeeHistorianRecordMsg(int Type);
	const char *FailedMsgOn() const;
};
	""")

	print("#endif // GAME_GENERATED_PROTOCOL_H")


def gen_network_source():

	print("""\
#include "protocol.h"

#include <base/system.h>
#include <engine/shared/packer.h>
#include <engine/shared/protocol.h>
#include <engine/shared/uuid_manager.h>

#include <game/mapitems_ex.h>

CNetObjHandler::CNetObjHandler()
{
	m_pMsgFailedOn = "";
	m_pObjFailedOn = "";
	m_pObjCorrectedOn = "";
	m_NumObjCorrections = 0;
}

int CNetObjHandler::NumObjCorrections() const { return m_NumObjCorrections; }
const char *CNetObjHandler::CorrectedObjOn() const { return m_pObjCorrectedOn; }
const char *CNetObjHandler::FailedObjOn() const { return m_pObjFailedOn; }
const char *CNetObjHandler::FailedMsgOn() const { return m_pMsgFailedOn; }

static const int max_int = 0x7fffffff;
static const int min_int = 0x80000000;

int CNetObjHandler::ClampInt(const char *pErrorMsg, int Value, int Min, int Max)
{
	if(Value < Min) { m_pObjCorrectedOn = pErrorMsg; m_NumObjCorrections++; return Min; }
	if(Value > Max) { m_pObjCorrectedOn = pErrorMsg; m_NumObjCorrections++; return Max; }
	return Value;
}
	""")


	lines = []
	lines += ["const char *CNetObjHandler::ms_apObjNames[] = {"]
	lines += ['\t"EX/UUID",']
	lines += [f'\t"{o.name}",' for o in network.Objects if o.ex is None]
	lines += ['\t""', "};", ""]

	lines += ["const char *CNetObjHandler::ms_apExObjNames[] = {"]
	lines += ['\t"invalid",']
	lines += [f'\t"{o.name}",' for o in network.Objects if o.ex is not None]
	lines += ['\t""', "};", ""]

	lines += ["int CNetObjHandler::ms_aObjSizes[] = {"]
	lines += ['\t0,']
	lines += [f'\tsizeof({o.struct_name}),' for o in network.Objects if o.ex is None]
	lines += ['\t0', "};", ""]

	lines += ["int CNetObjHandler::ms_aUnpackedObjSizes[] = {"]
	lines += ['\t16,']
	lines += [f'\tsizeof({o.struct_name}),' for o in network.Objects if o.ex is None]
	lines += ["};", ""]

	lines += ["int CNetObjHandler::ms_aUnpackedExObjSizes[] = {"]
	lines += ['\t0,']
	lines += [f'\tsizeof({o.struct_name}),' for o in network.Objects if o.ex is not None]
	lines += ["};", ""]

	lines += ['const char *CNetObjHandler::ms_apMsgNames[] = {']
	lines += ['\t"invalid",']
	lines += [f'\t"{msg.name}",' for msg in network.Messages if msg.ex is None]
	lines += ['\t""', "};", ""]

	lines += ['const char *CNetObjHandler::ms_apExMsgNames[] = {']
	lines += ['\t"invalid",']
	lines += [f'\t"{msg.name}",' for msg in network.Messages if msg.ex is not None]
	lines += ['\t""', "};", ""]

	for line in lines:
		print(line)

	print("""\
const char *CNetObjHandler::GetObjName(int Type) const
{
	if(Type >= 0 && Type < NUM_NETOBJTYPES)
	{
		return ms_apObjNames[Type];
	}
	else if(Type > __NETOBJTYPE_UUID_HELPER && Type < OFFSET_NETMSGTYPE_UUID)
	{
		return ms_apExObjNames[Type - __NETOBJTYPE_UUID_HELPER];
	}
	return "(out of range)";
}

int CNetObjHandler::GetObjSize(int Type) const
{
	if(Type < 0 || Type >= NUM_NETOBJTYPES) return 0;
	return ms_aObjSizes[Type];
}

int CNetObjHandler::GetUnpackedObjSize(int Type) const
{
	if(Type >= 0 && Type < NUM_NETOBJTYPES)
	{
		return ms_aUnpackedObjSizes[Type];
	}
	else if(Type > __NETOBJTYPE_UUID_HELPER && Type < OFFSET_NETMSGTYPE_UUID)
	{
		return ms_aUnpackedExObjSizes[Type - __NETOBJTYPE_UUID_HELPER];
	}
	return 0;
}

const char *CNetObjHandler::GetMsgName(int Type) const
{
	if(Type >= 0 && Type < NUM_NETMSGTYPES)
	{
		return ms_apMsgNames[Type];
	}
	else if(Type > __NETMSGTYPE_UUID_HELPER && Type < OFFSET_MAPITEMTYPE_UUID)
	{
		return ms_apExMsgNames[Type - __NETMSGTYPE_UUID_HELPER];
	}
	return "(out of range)";
}
	""")

	lines = []
	lines += ["""\
void *CNetObjHandler::SecureUnpackObj(int Type, CUnpacker *pUnpacker)
{
	m_pObjFailedOn = 0;
	switch(Type)
	{
	case NETOBJTYPE_EX:
	{
		const unsigned char *pPtr = pUnpacker->GetRaw(sizeof(CUuid));
		if(pPtr != 0)
		{
			mem_copy(m_aUnpackedData, pPtr, sizeof(CUuid));
		}
		break;
	}
	"""]

	for item in network.Objects:
		for line in item.emit_uncompressed_unpack_and_validate(network.Objects):
			lines += ["\t" + line]
		lines += ['\t']

	lines += ["""\
	default:
		m_pObjFailedOn = "(type out of range)";
		break;
	}
	
	if(pUnpacker->Error())
		m_pObjFailedOn = "(unpack error)";
	
	if(m_pObjFailedOn)
		return 0;
	m_pObjFailedOn = "";
	return m_aUnpackedData;
}
	"""]
	for line in lines:
		print(line)


	lines = []
	lines += ["""\
void *CNetObjHandler::SecureUnpackMsg(int Type, CUnpacker *pUnpacker)
{
	m_pMsgFailedOn = 0;
	switch(Type)
	{
	"""]

	for item in network.Messages:
		for line in item.emit_unpack_msg():
			lines += ["\t" + line]
		lines += ['\t']

	lines += ["""\
	default:
		m_pMsgFailedOn = "(type out of range)";
		break;
	}
	
	if(pUnpacker->Error())
		m_pMsgFailedOn = "(unpack error)";
	
	if(m_pMsgFailedOn)
		return 0;
	m_pMsgFailedOn = "";
	return m_aUnpackedData;
}
	"""]
	for line in lines:
		print(line)


	lines = []
	lines += ["""\
bool CNetObjHandler::TeeHistorianRecordMsg(int Type)
{
	switch(Type)
	{
	"""]

	empty = True
	for msg in network.Messages:
		if not msg.teehistorian:
			lines += [f'\tcase {msg.enum_name}:']
			empty = False
	if not empty:
		lines += ['\t\treturn false;']

	lines += ["""\
	default:
		return true;
	}
}
	"""]
	for line in lines:
		print(line)


	lines = []
	lines += ["""\
void RegisterGameUuids(CUuidManager *pManager)
{
	"""]

	for item in network.Objects + network.Messages:
		if item.ex is not None:
			lines += [f'\tpManager->RegisterName({item.enum_name}, "{item.ex}");']

	lines += ["""
	RegisterMapItemTypeUuids(pManager);
}
	"""]
	for line in lines:
		print(line)


def gen_common_content_types_header():
	# print some includes
	print('#include <engine/graphics.h>')

	# emit the type declarations
	with open("datasrc/content.py", "rb") as content_file:
		contentlines = content_file.readlines()
		order = []
		for line in contentlines:
			line = line.strip()
			if line[:6] == "class ".encode() and "(Struct)".encode() in line:
				order += [line.split()[1].split("(".encode())[0].decode("ascii")]
		for name in order:
			EmitTypeDeclaration(content.__dict__[name])


def gen_common_content_header():
	# print some includes
	print('#include "data_types.h"')

	# the container pointer
	print('extern CDataContainer *g_pData;')

	# enums
	EmitEnum([f"IMAGE_{i.name.value.upper()}" for i in content.container.images.items], "NUM_IMAGES")
	EmitEnum([f"ANIM_{i.name.value.upper()}" for i in content.container.animations.items], "NUM_ANIMS")
	EmitEnum([f"SPRITE_{i.name.value.upper()}" for i in content.container.sprites.items], "NUM_SPRITES")

def gen_common_content_source():
	EmitDefinition(content.container, "datacontainer")
	print('CDataContainer *g_pData = &datacontainer;')


def gen_content_types_header():
	print("#ifndef CONTENT_TYPES_HEADER")
	print("#define CONTENT_TYPES_HEADER")
	gen_common_content_types_header()
	print("#endif")


def gen_client_content_header():
	print("#ifndef CLIENT_CONTENT_HEADER")
	print("#define CLIENT_CONTENT_HEADER")
	gen_common_content_header()
	print("#endif")


def gen_client_content_source():
	print('#include "client_data.h"')
	gen_common_content_source()


def gen_server_content_header():
	print("#ifndef SERVER_CONTENT_HEADER")
	print("#define SERVER_CONTENT_HEADER")
	gen_common_content_header()
	print("#endif")


def gen_server_content_source():
	print('#include "server_data.h"')
	gen_common_content_source()


def main():
	parser = argparse.ArgumentParser(
		description=('Generate C++ Source Files for the Teeworlds Network Protocol')
	)
	FUNCTION_MAP = {
						'network_header': gen_network_header,
						'network_source': gen_network_source,
						'content_types_header': gen_content_types_header,
						'client_content_header': gen_client_content_header,
						'client_content_source': gen_client_content_source,
						'server_content_header': gen_server_content_header,
						'server_content_source': gen_server_content_source,
					}

	parser.add_argument('file_to_generate', choices=FUNCTION_MAP.keys())
	args = parser.parse_args()
	FUNCTION_MAP[args.file_to_generate]()

if __name__ == '__main__':
	main()
