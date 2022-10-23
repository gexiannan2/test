#include "RedisCtrl.h"
#include "../Game/handler.h"
#include "../Game/LoadConfig.h"
#include "../Game/GameCtrl.h"
#include "../Game/Player.h"
#include "../protocol/Redisprotocol.h"
#include "../KuaFuWar/SubWar.h"
#include "../protocol/prompt.h"

#include <sstream>

IMPLEMENT_SINGLETON(CRedisCtrl);

CRedisCtrl::CRedisCtrl()
{
	SysInitLock(&m_oLock);
}

CRedisCtrl::~CRedisCtrl()
{
	SysCleanLock(&m_oLock);

	std::list<STRedisData*>::iterator itr = m_NetEvnetQueue.begin();
	while (itr != m_NetEvnetQueue.end())
	{
		SAFE_DELETE((*itr)->pOutParam);
		delete *itr;
		itr++;
	}
	m_NetEvnetQueue.clear();

	itr = m_ProcessEventQueue.begin();
	while (itr != m_ProcessEventQueue.end())
	{
		SAFE_DELETE((*itr)->pOutParam);
		delete *itr;
		itr++;
	}
	m_ProcessEventQueue.clear();
}

bool CRedisCtrl::Init()
{

	string strRedisIP;
	WORD   wRedisPort;
	string strChannel; //发布和订阅的频道
	string strMonitor; //监控频道
	string strPasswd;
	int nOpen = 0;

	///////////////////////////////////////////////
	string strCurrentDir = GetExePath();
	string strFileName = strCurrentDir + "/" + "rediscfg.ini";

	if (!m_oIniFile.LoadFile(strFileName.c_str()))
	{
		SYS_CRITICAL("[%s:%d]Load %s file failed!", MSG_MARK, strFileName.c_str());
		return false;
	}
	char szBuf[256] = { 0 };
	if (m_oIniFile.ReadString("RedisServer", "ip", "127.0.0.1", szBuf, 256))
	{
		strRedisIP = szBuf;
	}
	INT32 nDest = 0;
	if (m_oIniFile.ReadInteger("RedisServer", "port", 6379, nDest))
	{
		wRedisPort = nDest;
	}

	memset(szBuf, 0, sizeof(szBuf));
	if (m_oIniFile.ReadString("RedisServer", "passwd", "", szBuf, 256))
	{
		char alCode[1024];
		memset(alCode, 0, sizeof(alCode));

		CDBCode::DecodeDBcode(szBuf, strlen(szBuf), (unsigned char*)alCode);
		strPasswd = alCode;
	}

	memset(szBuf, 0, sizeof(szBuf));
	if (m_oIniFile.ReadString("RedisServer", "channel", "helloworld", szBuf, 256))
	{
		strChannel = szBuf;
	}

	memset(szBuf, 0, sizeof(szBuf));
	if (m_oIniFile.ReadString("RedisServer", "monitor", "helloworld", szBuf, 256))
	{
		strMonitor = szBuf;
	}

	nDest = 0;
	if (m_oIniFile.ReadInteger("RedisServer", "open", 0, nDest))
	{
		nOpen = nDest;
	}

	if (nOpen==0)
	{
		CLog::ErrorLog("redis do not open!", MSG_MARK);
		return true;
	}
	///////////////////////////

	if (!m_RedisSession.Init(strRedisIP,wRedisPort,strPasswd,strChannel, strMonitor))
	{
		CLog::ErrorLog("[%s:%d]start the thread failed!", MSG_MARK);
		return false;
	}

	if (!m_SubRedisSession.Init(strRedisIP, wRedisPort, strPasswd, strChannel, strMonitor))
	{
		CLog::ErrorLog("[%s:%d]start the thread2 failed!", MSG_MARK);
		return false;
	}

	m_dwLastReportTime = time(NULL);
	return true;
}

void CRedisCtrl::UnInit()
{
	m_RedisSession.Terminate();
	m_SubRedisSession.Terminate();
	m_RedisSession.Wait();
	//m_SubRedisSession.Wait();
}

bool CRedisCtrl::ProcessEvent(INT32 nProcessNum)
{
	TRY_START
	bool bPop = false;
	if (m_ProcessEventQueue.size() == 0)
	{
		bPop = true;
		if (!PopEvent())
			return false;
	}

	m_nProcessCount = 0;

	for (int i = 0; i < 2; ++i)
	{
		std::list<STRedisData*>::iterator itr = m_ProcessEventQueue.begin();
		while (itr != m_ProcessEventQueue.end())
		{
			if (*itr)
			{
				////////////
				ProcessPacket(*itr);
				////////////
				m_nProcessCount++;
			}

			delete (*itr);
			itr = m_ProcessEventQueue.erase(itr);

			if (nProcessNum > 0 && m_nProcessCount >= nProcessNum)
				return true;
		}
		if (bPop)
		{
			break;
		}
		if (m_ProcessEventQueue.size() == 0)
		{
			bPop = true;
			if (!PopEvent())
				return false;
		}
	}


	TRY_STOP
	return false;
}

void CRedisCtrl::PushEvent(STRedisData* pEvent)
{
	CAutoLock guard(&m_oLock);
	m_NetEvnetQueue.push_back(pEvent);
}

bool CRedisCtrl::PopEvent()
{
	CAutoLock guard(&m_oLock);

	if (m_NetEvnetQueue.size() > 0)
	{
		m_ProcessEventQueue.clear();
		m_ProcessEventQueue.swap(m_NetEvnetQueue);
		return true;
	}
	return false;
}

bool CRedisCtrl::IsConnect()
{
	return m_RedisSession.IsConnect();
}


void CRedisCtrl::ProcessPacket(STRedisData* pEvent)
{
	TRY_START
	if (pEvent->pFunc)
	{
		switch (pEvent->byType)
		{
		case E_RedisData_Normal:
			{
				CPlayer *pPlayer = NULL;
				if (pEvent->qwPlayerid > 0)
				{
					pPlayer = gpPlayerMapCtrl->Find(pEvent->qwPlayerid);
					if (!pPlayer)
					{
						SAFE_DELETE(pEvent->pOutParam);
						return;
					}
				}
				((P_REDIS_CALLBACKFUNC)pEvent->pFunc)(pPlayer, pEvent->pInParam, (vector<string>*)pEvent->pOutParam);
				SAFE_DELETE(pEvent->pOutParam);
			}
			break;
		case E_RedisData_Sub:
			{
				if (pEvent->pOutParam)
				{
					vector<string>* pOutVec = (vector<string>*)pEvent->pOutParam;
					if (pOutVec->size()>=3)
					{
						((P_SUBREDIS_CALLBACKFUNC)pEvent->pFunc)(pEvent->pSession, (*pOutVec)[2].c_str(), (*pOutVec)[2].size());
						
					}
					SAFE_DELETE(pEvent->pOutParam);
				}
			}
			break;
		default:
			break;
		}
		
	}
	TRY_STOP
}

void CRedisCtrl::ProcessSubPacket(CRedisSession* pSubSession, const char *szBuf, int nLen)
{
	TRY_START
	gpRedisParsers->ProcessPacket(pSubSession, szBuf, nLen);
	TRY_STOP
}

void CRedisCtrl::PublishData(int nServerId, string &strPacket)
{
	char szChannel[20];
	snprintf(szChannel, sizeof(szChannel), "%d", nServerId);
	m_RedisSession.AsyncSetValue(0, NULL, NULL, ES_PUBLISH, szChannel, strPacket);
}

void CRedisCtrl::PublishDataToAllServer(string &strPacket)
{
	m_RedisSession.AsyncSetValue(0, NULL, NULL, ES_PUBLISH, m_RedisSession.GetChannel(), strPacket);
}

//////////////////////////////////

// 发送沙巴克占领行会
void CRedisCtrl::SendLocalGsSubKing(int nServerId, std::string sGuildName)
{
	string strPacket = gpRedisPacketBuilder->BuildSendSubKing(nServerId, sGuildName);
	if (strPacket.size())
	{
		PublishData(nServerId, strPacket);
	}
}

void CRedisCtrl::SendLocalGsEmail(int nServerId, UINT64 qwPlayerId, string &strTitle, string &strContent, std::vector<EmailItem> &listPrize) //发送邮件
{
	string strPacket = gpRedisPacketBuilder->BuildSendEmail(nServerId, qwPlayerId, strTitle, strContent, listPrize);
	if (strPacket.size())
	{
		PublishData(nServerId,strPacket);
	}
}

void CRedisCtrl::SendKuaFuJumpReq(int nServerId,int nKFType, CPlayer *pPlayer, string strMapCode, string &strPlayerData)
{
	string strPacket = gpRedisPacketBuilder->BuildSendKuaFuInfoReq(nServerId, nKFType, pPlayer, strMapCode, strPlayerData);
	if (strPacket.size())
	{
		PublishData(nServerId, strPacket);
	}
}

void CRedisCtrl::SendLocalGsJumpReqAck(int nServerId, int nKFType, UINT64 qwPlayerId, int nRet, string strJumpMapCode)
{
	string strPacket = gpRedisPacketBuilder->BuildSendKuaFuInfoAck(nServerId, nKFType, qwPlayerId, nRet);
	if (strPacket.size())
	{
		PublishData(nServerId, strPacket);
	}
}

void CRedisCtrl::SendKuaFuKickPlayer(int nServerId, UINT64 qwPlayerId, string strPtID)
{
	string strPacket = gpRedisPacketBuilder->BuildSendKuaFuKickPlayer(nServerId, qwPlayerId, strPtID);
	if (strPacket.size())
	{
		PublishData(nServerId, strPacket);
	}
}

//void CRedisCtrl::SendKuaFuSubYB(int nServerId, UINT64 qwPlayerId, int nCount)
//{
//	string strPacket = gpRedisPacketBuilder->BuildSendKuaFuSubYB(nServerId, qwPlayerId, nCount);
//	if (strPacket.size())
//	{
//		PublishData(nServerId, strPacket);
//	}
//}

void CRedisCtrl::SendMonitorServerStatus(int nStatus)
{
	string strPacket = gpRedisPacketBuilder->BuildSendMonitorServerStatus(nStatus);
	if (strPacket.size())
	{
		m_RedisSession.AsyncSetValue(0, NULL, NULL, ES_PUBLISH, m_RedisSession.GetMonitor(), strPacket);
	}
}

void CRedisCtrl::SendMonitorErrorMsg(int nType, string strMsg)
{
	string strPacket = gpRedisPacketBuilder->BuildSendMonitorServerErrorMsg(nType, strMsg);
	if (strPacket.size())
	{
		m_RedisSession.AsyncSetValue(0, NULL, NULL, ES_PUBLISH, m_RedisSession.GetMonitor(), strPacket);
	}
}

void CRedisCtrl::ReportPlayerNum(time_t dwNowTime)
{
	if (difftime(dwNowTime, m_dwLastReportTime) > 300)
	{
		string strPacket = gpRedisPacketBuilder->BuildSendMonitorServerStatus(0);
		if (strPacket.size())
		{
			m_RedisSession.AsyncSetValue(0, NULL, NULL, ES_PUBLISH, m_RedisSession.GetMonitor(), strPacket);
		}
		m_dwLastReportTime = dwNowTime;
	}
}

void CRedisCtrl::SendKuaFuOffLine(int nServerId)
{
	string strPacket = gpRedisPacketBuilder->BuildSendOffLine(nServerId);
	if (strPacket.size())
	{
		PublishData(nServerId, strPacket);
	}
}

void CRedisCtrl::SendKFContent(std::string& str)
{
	PublishDataToAllServer(gpRedisPacketBuilder->CrossContent(0 ,str));
}

void CRedisCtrl::SendLocalGsKFYuanBaoConsume(int nServerId, UINT64 qwPlayerId, std::string strOrderId,int nOperation, DWORD dwCount)
{
	string strPacket = gpRedisPacketBuilder->KFYuanBaoConsume(nServerId,qwPlayerId,strOrderId ,dwCount,nOperation);
	if (strPacket.size())
	{
		PublishData(nServerId,strPacket);
	}
}

void CRedisCtrl::SendKFYuanBaoResult(int nServerId, UINT64 qwPlayerId, std::string strOrderId, DWORD dwResult ,int nOperation)
{
	string strPacket = gpRedisPacketBuilder->KFYuanBaoResult(nServerId, qwPlayerId, strOrderId, dwResult ,nOperation);
	if (strPacket.size())
	{
		PublishData(nServerId, strPacket);
	}
}

void CRedisCtrl::SendLocalRollbackKFYuanBao(int nServerId, UINT64 qwPlayerId, std::string strOrderId, DWORD dwCount ,DWORD dwIsRollBack)
{
	string strPacket = gpRedisPacketBuilder->KFYuanBaoRollBack(nServerId, qwPlayerId, strOrderId, dwCount,dwIsRollBack);
	if (strPacket.size())
	{
		PublishData(nServerId, strPacket);
	}
}
/////////////////////////////////


CRedisSession::CRedisSession()
{
	m_bTerminated = false;
	m_wRedisPort = 6379;
	m_rc = NULL;
	m_bConnect = false;
	SysInitLock(&m_oLock);
	m_nProcessCount = 0;

}

CRedisSession::~CRedisSession()
{
	SysCleanLock(&m_oLock);
}

void CRedisSession::Terminate()
{
	m_bTerminated = true;
}

bool CRedisSession::Init(string &strRedisIP, WORD wPort, string &strPasswd, string &strChannel, string &strMonitor)
{
	if (!Start())
	{
		SYS_CRITICAL("[%s:%d]start the redis thread failed!", MSG_MARK);
		return false;
	}
	m_strRedisIP = strRedisIP;
	m_wRedisPort = wPort;
	m_strPasswd = strPasswd;
	m_strChannel = strChannel;
	m_strMonitor = strMonitor;
	return true;
}

void CRedisSession::Uninit()
{
	if (m_rc)
	{
		//把没发送的发完
		ProcessEvent();
	}

	Close();
}

void CRedisSession::ThrdProc()
{
	bool isBusy;
	while (!m_bTerminated)
	{
		if (!m_bConnect)
		{
			if (!Connect())
			{
				SYS_CRITICAL("[%s:%d]connect redis %s %d failed!", MSG_MARK, m_strRedisIP.c_str(), m_wRedisPort);
				Sleep(1000);
				continue;
			}
		}
		
		isBusy = ProcessEvent(50);
		if (!isBusy)
			Sleep(200);
		else
			Sleep(10);

	}

	Uninit();
}

bool CRedisSession::Connect()
{
	TRY_START
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	m_rc = redisConnectWithTimeout(m_strRedisIP.c_str(), m_wRedisPort, tv);
	//m_rc = redisConnect(m_strRedisIP.c_str(), m_wRedisPort);
	if (m_rc == NULL || m_rc->err)
	{
		return false;
	}

	redisReply *reply = (redisReply*)redisCommand(m_rc, "AUTH %s",m_strPasswd.c_str());
	if (!reply || reply->type == REDIS_REPLY_ERROR)
	{
		SYS_CRITICAL("[%s:%d]auth redis %s %d failed!", MSG_MARK, m_strRedisIP.c_str(), m_wRedisPort);
		if(reply)
			freeReplyObject(reply);
		Close();
		return false;
	}

	OnConnect();
	return true;
	TRY_STOP
	return false;
}

void CRedisSession::Close()
{
	if (m_rc)
	{
		redisFree(m_rc);
		OnDisConnect();
	}
}

void CRedisSession::OnConnect()
{
	m_bConnect = true;

	CLog::ErrorLog("redis connected %s",m_strRedisIP.c_str());
}

void CRedisSession::OnDisConnect()
{
	m_bConnect = false;
	m_rc = NULL;

	CLog::ErrorLog("close redis %s", m_strRedisIP.c_str());
}


bool CRedisSession::ProcessEvent(INT32 nProcessNum)
{
	TRY_START
	bool bPop = false;
	if (m_ProcessEventQueue.size() == 0)
	{
		bPop = true;
		if (!PopEvent())
			return false;
	}

	m_nProcessCount = 0;

	for (int i = 0; i < 2;++i)
	{
	
		std::list<STRedisData*>::iterator itr = m_ProcessEventQueue.begin();
		while (itr != m_ProcessEventQueue.end())
		{
			if (!m_rc)
				break;
			
			if ((*itr))
			{
				////////////
				if (!ProcessPacket(*itr))
					break;
				////////////
				m_nProcessCount++;
			}
			itr = m_ProcessEventQueue.erase(itr);

			if (nProcessNum > 0 && m_nProcessCount >= nProcessNum)
				return true;
		}
		if (bPop)
		{
			break;
		}
		if (m_ProcessEventQueue.size() == 0)
		{
			bPop = true;
			if (!PopEvent())
				return false;
		}
	}
	
	
	TRY_STOP
	return false;
}

void CRedisSession::PushEvent(STRedisData* pEvent)
{
	CAutoLock guard(&m_oLock);
	m_NetEvnetQueue.push_back(pEvent);
}

bool CRedisSession::PopEvent()
{
	CAutoLock guard(&m_oLock);

	if (m_NetEvnetQueue.size() > 0)
	{
		m_ProcessEventQueue.clear();
		m_ProcessEventQueue.swap(m_NetEvnetQueue);
		return true;
	}
	return false;
}

bool CRedisSession::ProcessPacket(STRedisData* pEvent)
{
	TRY_START
	redisReply *reply;
	switch (pEvent->byCmdType)
	{
		case ES_SETBLOB: 
		case ES_PUBLISH:
			reply = (redisReply*)redisCommand(m_rc, pEvent->strCmd.c_str(), pEvent->strValue.c_str(), pEvent->strValue.size());
			break;
		default:
			reply = (redisReply*)redisCommand(m_rc, pEvent->strCmd.c_str());
			break;
	}
	if (!reply)
	{
		CLog::ErrorLog("exe error1:%s", pEvent->strCmd.c_str());
		Close();
		return false;
	}

	if (reply->type == REDIS_REPLY_ERROR)
	{
		CLog::ErrorLog("exe error2:%s", pEvent->strCmd.c_str());
		freeReplyObject(reply);
		Close();
		return true;
	}

	//在这里赋值
	if (pEvent->pFunc)
	{
		switch (reply->type)
		{
		case REDIS_REPLY_INTEGER:
			{
				vector<string> *date = new vector<string>;
				stringstream strInteger;
				strInteger << reply->integer;
				date->push_back(strInteger.str());

				pEvent->pOutParam = (void*)date;
				pEvent->pSession = this;
				gpRedisCtrl->PushEvent(pEvent);
			}
			break;
		case REDIS_REPLY_STRING:
			{
				vector<string> *date = new vector<string>;
				date->push_back(string(reply->str,reply->len));

				pEvent->pOutParam = (void*)date;
				pEvent->pSession = this;
				gpRedisCtrl->PushEvent(pEvent);
			}			
			break;
		case REDIS_REPLY_ARRAY:
			{
				vector<string> *date = new vector<string>;
				for (int i = 0; i < (int)reply->elements; i++)
					date->push_back(string(reply->element[i]->str, reply->element[i]->len));

				pEvent->pOutParam = (void*)date;
				pEvent->pSession = this;
				gpRedisCtrl->PushEvent(pEvent);
			}
			break;
		default:
			SAFE_DELETE(pEvent);
			break;
		}	
	}
	else
	{
		SAFE_DELETE(pEvent);
	}
	freeReplyObject(reply);
	return true;

	TRY_STOP

	return false;
}



/////////////////////////////////

void CRedisSubSession::ThrdProc()
{
	string strSubChannel = string("subscribe ") + m_strChannel;

	char szMySubChannel[64];
	snprintf(szMySubChannel,sizeof(szMySubChannel),"subscribe %d", gpLoadConfig->m_wServerID);

	string strSubMonitor = string("subscribe ") + m_strMonitor;

	int nRet;
	redisReply *reply;
	while (!m_bTerminated)
	{
		if (!m_bConnect)
		{
			if (!Connect())
			{
				SYS_CRITICAL("[%s:%d]connect redis %s %d failed!", MSG_MARK, m_strRedisIP.c_str(), m_wRedisPort);
				Sleep(1000);
				continue;
			}

			//监听公共频道
			reply = (redisReply*)redisCommand(m_rc, strSubChannel.c_str());
			if (!reply || reply->type == REDIS_REPLY_ERROR)
			{
				if (reply)
					freeReplyObject(reply);
				Close();
				SYS_CRITICAL("%s %s %d failed!", strSubChannel.c_str(), m_strRedisIP.c_str(), m_wRedisPort);
				Sleep(1000);
				continue;
			}

			SYS_CRITICAL("%s success!", strSubChannel.c_str());

			freeReplyObject(reply);

			//监听自己的频道
			reply = (redisReply*)redisCommand(m_rc, szMySubChannel);
			if (!reply || reply->type == REDIS_REPLY_ERROR)
			{
				if (reply)
					freeReplyObject(reply);
				Close();
				SYS_CRITICAL("%s %s %d failed!", szMySubChannel, m_strRedisIP.c_str(), m_wRedisPort);
				Sleep(1000);
				continue;
			}

			SYS_CRITICAL("%s success!", szMySubChannel);

			//监听监控频道
			reply = (redisReply*)redisCommand(m_rc, strSubMonitor.c_str());
			if (!reply || reply->type == REDIS_REPLY_ERROR)
			{
				if (reply)
					freeReplyObject(reply);
				Close();
				SYS_CRITICAL("%s %s %d failed!", strSubMonitor.c_str(), m_strRedisIP.c_str(), m_wRedisPort);
				Sleep(1000);
				continue;
			}

			SYS_CRITICAL("%s success!", strSubMonitor.c_str());

			freeReplyObject(reply);
		}

		do 
		{

			if((nRet=redisGetReply(m_rc, (void **)&reply)) == REDIS_OK) //会阻塞吗
			{
				if (reply == NULL || reply->type == REDIS_REPLY_ERROR)
				{
					if (reply)
						freeReplyObject(reply);
					Close();
					SYS_CRITICAL("[%s:%d]subscribe redis %s %d failed!", MSG_MARK, m_strRedisIP.c_str(), m_wRedisPort);
					Sleep(100);
					break;
				}
				switch (reply->type)
				{
				case REDIS_REPLY_ARRAY:
				{
					STRedisData *pSTRedisData = new STRedisData;
					vector<string> *date = new vector<string>;
					for (int i = 0; i < (int)reply->elements; i++)
						date->push_back(string(reply->element[i]->str, reply->element[i]->len));

					pSTRedisData->pFunc = CRedisCtrl::ProcessSubPacket;
					pSTRedisData->byType = E_RedisData_Sub;
					pSTRedisData->pOutParam = (void*)date;
					pSTRedisData->pSession = this;
					gpRedisCtrl->PushEvent(pSTRedisData);
				}
				break;
				default:
					break;
				}

				freeReplyObject(reply);
			}
			else
			{
				Close();
				SYS_CRITICAL("[%s:%d]subscribe redis %s %d failed!", MSG_MARK, m_strRedisIP.c_str(), m_wRedisPort);
				Sleep(100);
				break;
			}

		} while (!m_bTerminated);

		Sleep(100);
	}

	Uninit();
}

void CRedisSubSession::Terminate()
{
	m_bTerminated = true;
	CSDThread::Terminate();
}