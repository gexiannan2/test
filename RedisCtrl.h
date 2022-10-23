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
	ES_SET,			//设置字符串
	ES_SETBLOB,		//设置二进制流
	ES_HSET,		//设置hash
	ES_LPUSH,		//设置列表
	ES_SADD,		//设置集合
	ES_ZADD,        //设置有序集合
	ES_PUBLISH,		//发布
	ES_HMSET,		//设置多key哈希
};

enum RedisGetDateType
{
	EG_GET,			//获取字符串
	EG_HGET,		//获取hash
	EG_ZRANK,		//获取元素排名
	EG_ZSCORE,		//获取元素分数
	EG_ZREVRANK,	//返回有序集合中指定成员的排名
	EG_ZREVRANGEBYSCORE,	//返回有序集中指定分数区间内的成员，分数从高到低排序
	EG_ZREVRANGE,	//返回有序集中指定区间内的成员，通过索引，分数从高到低
	EG_HGETALL,		//获取在哈希表中指定 key 的所有字段和值
	EG_HMGET,		//获取哈希表中多个值
};

class CRedisSession;
class CRedisSubSession;
///////////////////////////
typedef void(*P_REDIS_CALLBACKFUNC)(CPlayer*, void*, vector<string>*);
typedef void(*P_SUBREDIS_CALLBACKFUNC)(CRedisSession*, const char*, int);

enum RedisDataType
{
	E_RedisData_Normal = 1, //普通查询设置，发布
	E_RedisData_Sub = 2,    //订阅
};

//redis数据
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
//订阅类
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
	void	PublishData(int nServerId, string &strPacket); //发布消息
	void    PublishDataToAllServer(string &strPacket); //全服广播

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
	//发给跨服的用SendKuaFu这个开头
	//跨服发给本地的用SendLocalGs这个开头
	/////////////
	void SendLocalGsEmail(int nServerId, UINT64 qwPlayerId, string &strTitle, string &strContent, std::vector<EmailItem> &listPrize); //发送邮件
	void SendKuaFuJumpReq(int nServerId, int nKFType, CPlayer *pPlayer, string strMapCode, string &strPlayerData); //发送跨服请求
	void SendLocalGsJumpReqAck(int nServerId, int nKFType, UINT64 qwPlayerId, int nRet, string strJumpMapCode=""); //回应跨服请求
	void SendKuaFuKickPlayer(int nServerId, UINT64 qwPlayerId, string strPtID); //跨服踢人
	//void SendKuaFuSubYB(int nServerId, UINT64 qwPlayerId, int nCount);
	void SendMonitorServerStatus(int nStatus);
	void SendMonitorErrorMsg(int nType, string strMsg);
	void ReportPlayerNum(time_t dwNowTime);
	void SendKuaFuOffLine(int nServerId);
	// 发送沙巴克占领行会
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