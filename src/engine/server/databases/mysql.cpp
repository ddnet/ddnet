#include "mysql.h"

#include <cppconn/driver.h>

CMysqlConnection::CMysqlConnection(
		const char *pDatabase,
		const char *pPrefix,
		const char *pUser,
		const char *pPass,
		const char *pIp,
		int Port,
		bool Setup) :
	IDbConnection(pPrefix),
	m_pDriver(nullptr),
	m_Port(Port),
	m_Setup(Setup),
	m_InUse(false)
{
	str_copy(m_aDatabase, pDatabase, sizeof(m_aDatabase));
	str_copy(m_aUser, pUser, sizeof(m_aUser));
	str_copy(m_aPass, pPass, sizeof(m_aPass));
	str_copy(m_aIp, pIp, sizeof(m_aIp));
}

CMysqlConnection::~CMysqlConnection()
{
}

CMysqlConnection *CMysqlConnection::Copy()
{
	return new CMysqlConnection(m_aDatabase, m_aPrefix, m_aUser, m_aPass, m_aIp, m_Port, m_Setup);
}

IDbConnection::Status CMysqlConnection::Connect()
{
	if(m_InUse.exchange(true))
		return Status::IN_USE;

	return Status::ERROR;
}

void CMysqlConnection::Disconnect()
{
	m_InUse.store(false);
}

void CMysqlConnection::Lock()
{
}

void CMysqlConnection::Unlock()
{
}

void CMysqlConnection::PrepareStatement(const char *pStmt)
{
}

void CMysqlConnection::BindString(int Idx, const char *pString)
{
}

void CMysqlConnection::BindInt(int Idx, int Value)
{
}

void CMysqlConnection::Execute()
{
}

bool CMysqlConnection::Step()
{
	return false;
}

int CMysqlConnection::GetInt(int Col) const
{
	return 0;
}

void CMysqlConnection::GetString(int Col, char *pBuffer, int BufferSize) const
{
}

int CMysqlConnection::GetBlob(int Col, unsigned char *pBuffer, int BufferSize) const
{
	return 0;
}
