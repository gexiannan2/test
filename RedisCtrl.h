#pragma once
#include "stdafx.h"
#include "globalsetting.h"
#include "../Game/netevent.h"
#include "../Game/Player.h"
#include "hiredis/hiredis.h"
#include "../Game/Email.h"
#include <map>
using namespace std;

enum RedisSetDateType
{
	ES_SET,			//�����ַ���
	ES_SETBLOB,		//���ö�������
	ES_HSET,		//����hash
	ES_LPUSH,		//�����б�
	ES_SADD,		//���ü���
	ES_ZADD,        //�������򼯺�
	ES_PUBLISH,		//����
	ES_HMSET,		//���ö�key��ϣ
};

enum RedisGetDateType
{
	EG_GET,			//��ȡ�ַ���
	EG_HGET,		//��ȡhash
	EG_ZRANK,		//��ȡԪ������
	EG_ZSCORE,		//��ȡԪ�ط���
	EG_ZREVRANK,	//�������򼯺���ָ����Ա������
	EG_ZREVRANGEBYSCORE,	//����������ָ�����������ڵĳ�Ա�������Ӹߵ�������
	EG_ZREVRANGE,	//����������ָ�������ڵĳ�Ա��ͨ�������������Ӹߵ���
	EG_HGETALL,		//��ȡ�ڹ�ϣ����ָ�� key �������ֶκ�ֵ
	EG_HMGET,		//��ȡ��ϣ���ж��ֵ
};

class CRedisSession;
class CRedisSubSession;
///////////////////////////
typedef void(*P_REDIS_CALLBACKFUNC)(CPlayer*, void*, vector<string>*);
typedef void(*P_SUBREDIS_CALLBACKFUNC)(CRedisSession*, const char*, int);

enum RedisDataType
{
	E_RedisData_Normal = 1, //��ͨ��ѯ���ã�����
	E_RedisData_Sub = 2,    //����
};

//redis����
struct STRedisData
{
	UINT64 qwPlayerid;
	void *pFunc;
	void *pInParam;
	void *pOutParam;
	string strCmd;
	string strValue;
	BYTE  byCmdType;
	BYTE  byType;
	CRedisSession *pSession;

	STRedisData()
	{
		qwPlayerid = 0;
		pFunc = NULL;
		pInParam = NULL;
		pOutParam = NULL;
		byType = 0;
		pSession = NULL;
	}
};

///////////////////////////

class CRedisSession : public CSDThread
{
public:
	CRedisSession();
	virtual ~CRedisSession();
	virtual void SSAPI Terminate();
	virtual bool Init(string &strRedisIP, WORD wPort, string &strPasswd, string &strChannel, string &strMonitor);
	virtual void Uninit();

	bool Connect();
	void Close();

	bool IsConnect() {
		return m_bConnect;
	}

	string GetChannel() { return m_strChannel; }
	string GetMonitor() { return m_strMonitor; }

	template<typename T, typename ...Ts>
	inline string ToString(T && arg, Ts && ...args)
	{
		stringstream streamArg;
		streamArg << std::forward<T>(arg);
		if (sizeof...(args) > 0)
			return streamArg.str() + " " + ToString(std::forward<Ts>(args) ...);
		return streamArg.str();
	}

	string ToString()
	{
		return "";
	}

	template<typename ...Ts>
	void AsyncSetValue(UINT64 qwPlayerid, P_REDIS_CALLBACKFUNC pFunc, void *pInParam, enum RedisSetDateType eType, string strKey, Ts && ...args)
	{
		STRedisData *pSTRedisData = new STRedisData;
		switch (eType)
		{
		case ES_SET:
			pSTRedisData->strCmd = string("SET ") + strKey + string(" ") + ToString(std::forward<Ts>(args) ...);
			break;
		case ES_SETBLOB:
		{
			pSTRedisData->strCmd = string("SET ") + strKey + string(" %b");
			pSTRedisData->strValue = ToString(std::forward<Ts>(args) ...);
		}
		break;
		case ES_HSET:
			pSTRedisData->strCmd = string("HSET ") + strKey + string(" ") + ToString(std::forward<Ts>(args) ...);
			break;
		case ES_LPUSH:
			pSTRedisData->strCmd = string("LPUSH ") + strKey + string(" ") + ToString(std::forward<Ts>(args) ...);
			break;
		case ES_SADD:
			pSTRedisData->strCmd = string("SADD ") + strKey + string(" ") + ToString(std::forward<Ts>(args) ...);
			break;
		case ES_ZADD:
			pSTRedisData->strCmd = string("ZADD ") + strKey + string(" ") + ToString(std::forward<Ts>(args) ...);
			break;
		case ES_PUBLISH:
		{
			pSTRedisData->strCmd = string("PUBLISH ") + strKey + string(" %b");
			pSTRedisData->strValue = ToString(std::forward<Ts>(args) ...);
		}
		break;
		case ES_HMSET:
			pSTRedisData->strCmd = string("HMSET ") + strKey + string(" ") + ToString(std::forward<Ts>(args) ...);
			break;
		default:
		{
			delete pSTRedisData;
			return;
		}
		break;
		}

		pSTRedisData->byCmdType = eType;
		pSTRedisData->byType = E_RedisData_Normal;
		pSTRedisData->pFunc = pFunc;
		pSTRedisData->qwPlayerid = qwPlayerid;
		pSTRedisData->pInParam = pInParam;

		PushEvent(pSTRedisData);
	}

	template<typename ...Ts>
	void AsyncGetValue(UINT64 qwPlayerid, P_REDIS_CALLBACKFUNC pFunc, void *pInParam, enum RedisGetDateType eType, Ts && ...args)
	{
		STRedisData *pSTRedisData = new STRedisData;
		switch (eType)
		{
		case EG_GET:
			pSTRedisData->strCmd = string("GET ") + ToString(std::forward<Ts>(args) ...);
			break;
		case EG_HGET:
			pSTRedisData->strCmd = string("HGET ") + ToString(std::forward<Ts>(args) ...);
			break;
		case EG_ZRANK:
			pSTRedisData->strCmd = string("ZRANK ") + ToString(std::forward<Ts>(args) ...);
			break;
		case EG_ZSCORE:
			pSTRedisData->strCmd = string("ZSCORE ") + ToString(std::forward<Ts>(args) ...);
			break;
		case EG_ZREVRANK:
			pSTRedisData->strCmd = string("ZREVRANK ") + ToString(std::forward<Ts>(args) ...);
			break;
		case EG_ZREVRANGEBYSCORE:
			pSTRedisData->strCmd = string("ZREVRANGEBYSCORE ") + ToString(std::forward<Ts>(args) ...);
			break;
		case EG_ZREVRANGE:
			pSTRedisData->strCmd = string("ZREVRANGE ") + ToString(std::forward<Ts>(args) ...);
			break;
		case EG_HGETALL:
			pSTRedisData->strCmd = string("HGETALL ") + ToString(std::forward<Ts>(args) ...);
			break;
		case EG_HMGET:
			pSTRedisData->strCmd = string("HMGET ") + ToString(std::forward<Ts>(args) ...);
			break;
		default:
		{
			delete pSTRedisData;
			return;
		}
		break;
		}

		pSTRedisData->byCmdType = eType;
		pSTRedisData->byType = E_RedisData_Normal;

		pSTRedisData->pFunc = pFunc;
		pSTRedisData->qwPlayerid = qwPlayerid;
		pSTRedisData->pInParam = pInParam;

		PushEvent(pSTRedisData);
	}

protected:
	virtual void SSAPI ThrdProc();

	virtual bool ProcessEvent(INT32 nProcessNum = 0);
	virtual void OnConnect();
	virtual void OnDisConnect();

private:
	void PushEvent(STRedisData* pEvent);
	bool PopEvent();
	bool ProcessPacket(STRedisData* pEvent);

protected:
	bool		  m_bTerminated;
	string        m_strRedisIP;
	WORD          m_wRedisPort;
	string		  m_strPasswd;
	string        m_strChannel;
	string        m_strMonitor;
	redisContext* m_rc;
	bool          m_bConnect;

	list<STRedisData*> m_NetEvnetQueue;
	list<STRedisData*> m_ProcessEventQueue;
	MUTEX_TYPE     m_oLock;
	int m_nProcessCount;

};


//////////////////////
//������
class CRedisSubSession : public CRedisSession
{
public:
	CRedisSubSession() {}
	~CRedisSubSession() {}

	virtual void SSAPI Terminate();

protected:

	virtual void SSAPI ThrdProc();
	
};

///////////////////////////

class CRedisCtrl
{
	DECLARE_SINGLETON(CRedisCtrl);

public:

	CRedisCtrl();
	~CRedisCtrl();

	bool Init();
	void UnInit();
	bool ProcessEvent(INT32 nProcessNum);
	void PushEvent(STRedisData* pEvent);
	bool IsConnect();

	static void ProcessSubPacket(CRedisSession* pSubSession, const char *szBuf, int nLen);
	void	PublishData(int nServerId, string &strPacket); //������Ϣ
	void    PublishDataToAllServer(string &strPacket); //ȫ���㲥

	template<typename ...Ts>
	void AsyncSetValue(UINT64 qwPlayerid, P_REDIS_CALLBACKFUNC pFunc, void *pInParam, enum RedisSetDateType eType, string strKey, Ts && ...args)
	{
		m_RedisSession.AsyncSetValue(qwPlayerid, pFunc, pInParam, eType, strKey, std::forward<Ts>(args) ...);
	}

	template<typename ...Ts>
	void AsyncGetValue(UINT64 qwPlayerid, P_REDIS_CALLBACKFUNC pFunc, void *pInParam, enum RedisSetDateType eType, string strKey, Ts && ...args)
	{
		m_RedisSession.AsyncGetValue(qwPlayerid, pFunc, pInParam, eType, strKey, std::forward<Ts>(args) ...);
	}

	///////////////////
	//�����������SendKuaFu�����ͷ
	//����������ص���SendLocalGs�����ͷ
	/////////////
	void SendLocalGsEmail(int nServerId, UINT64 qwPlayerId, string &strTitle, string &strContent, std::vector<EmailItem> &listPrize); //�����ʼ�
	void SendKuaFuJumpReq(int nServerId, int nKFType, CPlayer *pPlayer, string strMapCode, string &strPlayerData); //���Ϳ������
	void SendLocalGsJumpReqAck(int nServerId, int nKFType, UINT64 qwPlayerId, int nRet, string strJumpMapCode=""); //��Ӧ�������
	void SendKuaFuKickPlayer(int nServerId, UINT64 qwPlayerId, string strPtID); //�������
	//void SendKuaFuSubYB(int nServerId, UINT64 qwPlayerId, int nCount);
	void SendMonitorServerStatus(int nStatus);
	void SendMonitorErrorMsg(int nType, string strMsg);
	void ReportPlayerNum(time_t dwNowTime);
	void SendKuaFuOffLine(int nServerId);
	// ����ɳ�Ϳ�ռ���л�
	void SendLocalGsSubKing(int nServerId, std::string sGuildName);
	void SendKFContent(std::string& str);
	void SendLocalGsKFYuanBaoConsume(int nServerId, UINT64 qwPlayerId, std::string strOrderId, int nOperation ,DWORD dwCount = 0);
	void SendKFYuanBaoResult(int nServerId, UINT64 qwPlayerId, std::string strOrderId, DWORD dwResult ,int nOperation = 0 ); //dwResult 1 :success  0: fail 
	void SendLocalRollbackKFYuanBao(int nServerId, UINT64 qwPlayerId, std::string strOrderId, DWORD dwCount ,DWORD dwIsRollBack);
	
private:
	void ProcessPacket(STRedisData* pEvent);

	bool PopEvent();

public:
	CRedisSession  m_RedisSession;
	CRedisSubSession  m_SubRedisSession;

	CIniFile    m_oIniFile;

	//static void LoadKuaFuSubYB(CPlayer* pPlayer, void* pInParam, vector<string>* date);
private:
	list<STRedisData*> m_NetEvnetQueue;
	list<STRedisData*> m_ProcessEventQueue;
	MUTEX_TYPE     m_oLock;
	int m_nProcessCount;

	time_t m_dwLastReportTime;
};

#define gpRedisCtrl    (CRedisCtrl::Instance())

///////////////////////////