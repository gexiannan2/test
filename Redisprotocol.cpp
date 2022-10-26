#include "Redisprotocol.h"
#include "../netcmd/rediscmd.pb.h"
#include "../netcmd/RedisCmdDef.h"
#include "../netmodules/PipeMgr.h"
#include "../netmodules/RedisCtrl.h"

#include "../Game/Player.h"
#include "../Game/GameCtrl.h"
#include "../Game/GlobeDefines.h"
#include "../Game/LoadConfig.h"
#include "../Game/GlobeParam.h"
#include "../Game/GameCtrl.h"
#include "../GuildCtrl/GuildCtrl.h"
#include "../protocol/prompt.h"
#include "../protocol/PacketBuilders.h"
#include "../../common/PromptProtocol.h"
#include "../KuaFuWar/SubWar.h"
#include "../Game/YuanBao.h"
#include "../Game/Activity.h"



extern string g_strKFSubGuild;
extern string g_strKFSubGuildName;
extern int g_nKFSubGuildAreaId;

//////////////////////////////////////////////////////////////////////
IMPLEMENT_SINGLETON(CRedisParsers);

void CRedisParsers::ProcessPacket(CRedisSession* pSubSession, const char *pData, int nLen)
{
	TRY_START

	ReidsCmdHead stCmdHead;
	int nHeadLen = sizeof(ReidsCmdHead);
	if (nLen < nHeadLen)
		return;
	int nDataLen = nLen - nHeadLen;
	memcpy(&stCmdHead, pData, nHeadLen);
	if (stCmdHead.nDataLen != nDataLen)
		return;

	//是给自己的才处理
	if (stCmdHead.nToServerId>0 && stCmdHead.nToServerId != gpLoadConfig->m_wServerID)
	{
		return;
	}

#ifndef _DEBUG
	//是自己发的不处理
	if (stCmdHead.nFromServerId == gpLoadConfig->m_wServerID)
	{
		return;
	}
#endif

	switch (stCmdHead.wCmdCode)
	{
	case R_SEND_EMAIL:
		OnRecvSendEmail(stCmdHead.nFromServerId, pData + nHeadLen, nDataLen);
		break;
	case R_KUAFU_REQ:
		OnRecvKuaFuInfo(stCmdHead.nFromServerId, pData + nHeadLen, nDataLen);
		break;
	case R_KUAFU_ACK:
		OnRecvKuaFuInfoAck(stCmdHead.nFromServerId, pData + nHeadLen, nDataLen);
		break;
	case R_SEND_BROADCAST:
		OnRecvSendBroadcast(stCmdHead.nFromServerId, pData + nHeadLen, nDataLen);
		break;
	case R_KUAFU_KICK_PLAYER:
		OnRecvKuaFuKickPlayer(stCmdHead.nFromServerId, pData + nHeadLen, nDataLen);
		break;
	case R_KUAFU_SUB_YB:
	//	OnRecvKuaFuSubYB(stCmdHead.nFromServerId, pData + nHeadLen, nDataLen);
		break;
	case R_KUAFU_OFFLINE:
		OnRecvOffLine(stCmdHead.nFromServerId);
		break;
		// 跨服沙城得主
	case R_KUAFU_SUB_KING_GUILD:
		BuildSendKFSubKing(stCmdHead.nFromServerId, pData + nHeadLen, nDataLen);
		break;
	case R_KUAFU_CONTENT:
		OnRecvKuaFuContent(stCmdHead.nFromServerId, pData + nHeadLen, nDataLen);
		break;
	case R_KUAFU_YUANBAO:
		OnRecvKuaFuYuanBao(stCmdHead.nFromServerId, pData + nHeadLen, nDataLen);
		break;
	case R_KUAFU_YUANBAO_RESULT:
		OnRecvKuaFuYuanBaoResult(stCmdHead.nFromServerId, pData + nHeadLen, nDataLen);
		break;
	case R_KUAFU_YUANBAO_ROLLBACK:
		OnRecvKuaFuYuanBaoRollBack(stCmdHead.nFromServerId, pData + nHeadLen, nDataLen);
		break;
	}

	TRY_STOP
}

//收到发送邮件
void CRedisParsers::OnRecvSendEmail(int nFromServerID, const char *pData, int nLen)
{
	TRY_START
	RedisCmd::EmailInfo emailInfo;
	if (!emailInfo.ParseFromArray(pData, nLen))
	{
		return;
	}

	list<EMAIL_ATTACHMENT_INFO> conListItem;
	EMAIL_ATTACHMENT_INFO stInfo;
	for (int i = 0; i < emailInfo.infos_size(); ++i)
	{
		stInfo.itemType = emailInfo.infos(i).itemtype();
		stInfo.dwIdx = emailInfo.infos(i).itemidx();
		stInfo.dwCount = emailInfo.infos(i).itemnum();
		stInfo.byBind = emailInfo.infos(i).itembind();
		conListItem.push_back(stInfo);
	}
	//发邮件
	gpEmail->CreateSysSimpleAttachmentEmail(emailInfo.playerid(), emailInfo.title(), emailInfo.content(), conListItem);

	TRY_STOP
}

void CRedisParsers::OnRecvKuaFuInfo(int nFromServerID, const char * pData, int nLen)
{
	TRY_START
	RedisCmd::KuaFuInfoReq kuafuInfo;
	if (!kuafuInfo.ParseFromArray(pData, nLen))
	{
		return;
	}
	if (kuafuInfo.playerid() > 0)
	{
	
		if (kuafuInfo.roletype()==0) //是主体
		{
			PUSER pUser = gpUserNameMapCtrl->Find(kuafuInfo.ptid());
			if (pUser && pUser->pPlayer)
				gpPlayerMapCtrl->KickPlayer(pUser->pPlayer);
			else
			{
				CPlayer *pPlayer = gpPlayerMapCtrl->Find(kuafuInfo.playerid());
				if (pPlayer)
					gpPlayerMapCtrl->KickPlayer(pPlayer);
			}

			string strPlayerName = "[" + to_string(kuafuInfo.areagroup()) + "]" + kuafuInfo.playername();
			
			if (kuafuInfo.guildname().size() > 0)
			{
				string strGuildName = "[" + to_string(nFromServerID) + "]" + kuafuInfo.guildname();
				gpGuildCtrl->CreateGuildByKF(kuafuInfo.playerid(), strPlayerName, strGuildName, kuafuInfo.playerlevel(), kuafuInfo.sex(), kuafuInfo.job(), kuafuInfo.duty());

			}

			int dwKuafuMaxNum = 800;
			string strValue;
			if (gpGlobeParam->GetParamInMemory("KuafuMaxNum", strValue))
				dwKuafuMaxNum = atoi(strValue.c_str());

			if (gpPlayerMapCtrl->m_insPlayerMap.size() >= dwKuafuMaxNum)
			{
				gpRedisCtrl->SendLocalGsJumpReqAck(nFromServerID, kuafuInfo.jumptype(), kuafuInfo.playerid(), ES_KUAFU_FULL);
				return;
			}

			MyKuaFuPlayer kuafuplayer;
			kuafuplayer.dwTick = time(NULL);
			kuafuplayer.nFromServer = nFromServerID;
			kuafuplayer.qwPlayerid = kuafuInfo.playerid();
			kuafuplayer.nKFType = kuafuInfo.jumptype();
			kuafuplayer.strJumpMap = kuafuInfo.mapcode();
			m_KuaFuPlayer[strPlayerName] = kuafuplayer;
		}
	

		g_pDBController->SendData(kuafuInfo.playerdata().c_str(), kuafuInfo.playerdata().size(), SD_DB_SERVER);
		
	}
	TRY_STOP
}

void CRedisParsers::OnRecvKuaFuInfoAck(int nFromServerID, const char * pData, int nLen)
{
	TRY_START
	RedisCmd::KuaFuInfoAck kuafuInfoack;
	if (!kuafuInfoack.ParseFromArray(pData, nLen))
	{
		return;
	}

	CPlayer *pPlayer = gpPlayerMapCtrl->Find(kuafuInfoack.playerid());
	if (!pPlayer)
		return;
	switch (kuafuInfoack.ret())
	{
	case ES_KUAFU_ALLOW:
		{
			if (kuafuInfoack.roletype()==1) //是元神
			{

			}
			else
			{
				if (kuafuInfoack.jumptype() == E_KUAFUTYPE_SUBAK)
				{
					pPlayer->SendData(gpPacketBuilder->KuaFuAllow(pPlayer->m_dwAreaGroup, gpSubWarCtrl->m_KFSUBKServerInFo.strIP, gpSubWarCtrl->m_KFSUBKServerInFo.nPort, "[" + to_string(pPlayer->m_dwAreaGroup) + "]" + pPlayer->m_strName));
				}
				else
				{
					KuaFuServerInFo serverInfo;
					if (gpSubWarCtrl->GetKFServerInFo(kuafuInfoack.jumptype(), serverInfo))
					{
						pPlayer->SendData(gpPacketBuilder->KuaFuAllow(pPlayer->m_dwAreaGroup, serverInfo.strIP, serverInfo.nPort, "[" + to_string(pPlayer->m_dwAreaGroup) + "]" + pPlayer->m_strName));
					}
					else
					{
						pPlayer->SendSysMessage(P_KuaFu_Msg_No_Open, TALKTYPE_SYSTEM);
					}
				}
			}
			
		}		
		break;
	case ES_KUAFU_OFF:
		pPlayer->SendSysMessage(P_KuaFu_Msg_Off, TALKTYPE_SYSTEM);
		break;
	case ES_KUAFU_FULL:
		pPlayer->SendSysMessage(P_KuaFu_Msg_Full, TALKTYPE_SYSTEM);
		break;
	default:
		break;
	}

	TRY_STOP
}

void CRedisParsers::OnRecvKuaFuKickPlayer(int nFromServerID, const char * pData, int nLen)
{
	TRY_START
	RedisCmd::KuaFuKickPlayer kuafukickplayer;
	if (!kuafukickplayer.ParseFromArray(pData, nLen))
	{
		return;
	}

	if (kuafukickplayer.playerid() > 0)
	{
		PUSER pUser = gpUserNameMapCtrl->Find(kuafukickplayer.ptid());
		if (pUser && pUser->pPlayer)
			gpPlayerMapCtrl->KickPlayer(pUser->pPlayer);
		else
		{
			CPlayer *pPlayer = gpPlayerMapCtrl->Find(kuafukickplayer.playerid());
			if (pPlayer)
				gpPlayerMapCtrl->KickPlayer(pPlayer);
		}
	}
	TRY_STOP
}

//void CRedisParsers::OnRecvKuaFuSubYB(int nFromServerID, const char * pData, int nLen)
//{
	//TRY_START
	//	RedisCmd::KuaFuSubYB kuafusubyb;
	//if (!kuafusubyb.ParseFromArray(pData, nLen))
	//	return;

	//int nYuanBao = kuafusubyb.count();//扣除元宝
	//if (nYuanBao < 1)
	//	return;

	//CPlayer *pPlayer = gpPlayerMapCtrl->Find(kuafusubyb.playerid());
	//if (pPlayer)
	//{
	//	string strOrderID = "";
	//	string strEvent = "KFSUB";
	//	string strInfo = "";
	//	DWORD dwSubCount = 0;
	//	if (nYuanBao > pPlayer->GetObjProperty(Attr_YuanBao).Valdword)
	//	{
	//		dwSubCount = pPlayer->GetObjProperty(Attr_YuanBao).Valdword;//当前元宝

	//		if (gpYuanBao->SubOnlyYuanBaoCount(pPlayer, dwSubCount, strOrderID, strEvent, strInfo, YuanBaoCost_KFSUB))
	//		{
	//			int nRelease = nYuanBao - dwSubCount;
	//			//这里还要把剩余的加到内存和redis
	//			map<UINT64, int>::iterator mapIt = gpYuanBao->m_KuaFuSubYB.find(pPlayer->m_PlayerID);
	//			if (mapIt != gpYuanBao->m_KuaFuSubYB.end())
	//			{
	//				mapIt->second = nRelease;
	//			}
	//			else
	//			{
	//				gpYuanBao->m_KuaFuSubYB[pPlayer->m_PlayerID] = nRelease;
	//			}

	//			gpYuanBao->UpdateKFYB(pPlayer->m_PlayerID, nRelease);//元宝数更新数据库
	//		}
	//		else
	//		{
	//			dwSubCount = 0;
	//		}
	//	}
	//	else
	//	{
	//		if (gpYuanBao->SubOnlyYuanBaoCount(pPlayer, nYuanBao, strOrderID, strEvent, strInfo, YuanBaoCost_KFSUB))
	//		{
	//			dwSubCount = nYuanBao;
	//			gpYuanBao->UpdateKFYB(pPlayer->m_PlayerID, 0);//元宝数更新数据库
	//		}
	//	}

	//	if (dwSubCount)
	//	{
	//		char szMsg[200];
	//		snprintf(szMsg, sizeof(szMsg), gpPrompt->GetMsgString(P_KuaFu_Msg_Sub_YB).c_str(), dwSubCount);
	//		pPlayer->SendSysMessage(szMsg, TALKTYPE_SYSTEM);
	//	}
	//	
	//}
	//else
	//{
	//	//这里还要把剩余的加到内存和数据库
	//	map<UINT64, int>::iterator mapIt = gpYuanBao->m_KuaFuSubYB.find(kuafusubyb.playerid());
	//	if (mapIt != gpYuanBao->m_KuaFuSubYB.end())
	//	{
	//		mapIt->second += nYuanBao;
	//		gpYuanBao->UpdateKFYB(kuafusubyb.playerid(), mapIt->second);//元宝数写入数据库
	//	}
	//	else
	//	{
	//		gpYuanBao->m_KuaFuSubYB[kuafusubyb.playerid()] = nYuanBao;
	//		gpYuanBao->UpdateKFYB(kuafusubyb.playerid(), nYuanBao);//元宝数写入数据库
	//	}	
	//}
	//TRY_STOP
//}

//广播（播报）
void CRedisParsers::OnRecvSendBroadcast(int nFromServerID, const char *pData, int nLen)
{
	TRY_START
	RedisCmd::KuaFuBroadcast Info;
	if (!Info.ParseFromArray(pData, nLen))
	{
		return;
	}

	std::string newsId = Info.newsid();
	gpPlayerMapCtrl->SendScreenMessage(newsId);

	TRY_STOP
}

// 跨服沙城得主
void CRedisParsers::BuildSendKFSubKing(int nFromServerID, const char *pData, int nLen)
{
	TRY_START
	RedisCmd::KFSubKing kfSubKing;
	if (!kfSubKing.ParseFromArray(pData, nLen))
	{
		return;
	}

	std::string sGuildName = kfSubKing.guildname();
	g_strKFSubGuild = sGuildName;
	std::vector<std::string> strParams = ExtractStrings(sGuildName, "]");
	if (strParams.size() == 2 )
	{
		g_nKFSubGuildAreaId = atoi((strParams[0].substr(1, strParams[0].length() - 1)).c_str())  ;
		g_strKFSubGuildName = strParams[1];
	}

	gpGlobeParam->SetParam("KFSubGuild", sGuildName.c_str());
	TRY_STOP
}

void CRedisParsers::OnRecvKuaFuContent(int nFromServerID, const char *pData, int nLen)
{
	RedisCmd::KFCrossContent kfContent;
	if (!kfContent.ParseFromArray(pData, nLen))
	{
		return;
	}

	std::string strContent = kfContent.crosscontent();
	gpSubWarCtrl->WriteMessageToFile(strContent);
	gpSubWarCtrl->Init();
}

void CRedisParsers::OnRecvKuaFuYuanBao(int nFromServerID, const char *pData, int nLen)
{
#ifndef KUAFU_GS
	RedisCmd::KFYuanBaoConsume kfyuanbao;
	if (!kfyuanbao.ParseFromArray(pData, nLen))
	{
		return;
	}

	UINT64 qwPlayerId = kfyuanbao.playerid();
	std::string strOrder = kfyuanbao.strorder();
	DWORD dwCount = kfyuanbao.kfyuanbaocount();
	int nOperation = kfyuanbao.kfoperation();
	if (nOperation == 0) //consume
	{
		gpYuanBao->OnLocalKfYuanBao(nFromServerID, qwPlayerId, strOrder, dwCount);
	}
	else  if (nOperation == 1) //query
	{
		gpYuanBao->OnQueryLoaclKfYuanBao(nFromServerID, qwPlayerId, strOrder);
	}
#endif

}

void CRedisParsers::OnRecvKuaFuYuanBaoResult(int nFromServerID, const char * pData, int nLen)
{
#ifdef KUAFU_GS
	RedisCmd::KFYuanBaoResult result;
	if (!result.ParseFromArray(pData, nLen))
	{
		return;
	}
	
	UINT64 u64PlayerId = result.playerid();
	std::string strOrderID = result.strorder();
	DWORD dwResult= result.result();
	int nOperation = result.kfoperation();
	gpYuanBao->OnCrossKfYuanBao(nFromServerID, u64PlayerId, strOrderID ,dwResult ,nOperation);
#endif
}

void CRedisParsers::OnRecvKuaFuYuanBaoRollBack(int nFromServerID, const char * pData, int nLen)
{
#ifndef KUAFU_GS
	RedisCmd::KFYuanBaoConsume consume;
	if (!consume.ParseFromArray(pData, nLen))
	{
		return;
	}
	
	UINT64 u64PlayerId = consume.playerid();
	std::string strOrderID = consume.strorder();
	DWORD dwCount = consume.kfyuanbaocount();
	int nIsRollBack = consume.isrollback();
	gpYuanBao->OnKfYuanBaoRollBack(u64PlayerId, strOrderID ,dwCount ,nIsRollBack);
#endif
}


void CRedisParsers::OnRecvOffLine(int nFromServerID)
{
	vector<CPlayer *> vecPlayer;
	for (CPlayerIDMap::const_iterator itr = gpPlayerMapCtrl->m_insPlayerIDMap.begin();
	itr != gpPlayerMapCtrl->m_insPlayerIDMap.end(); ++itr)
	{
		CPlayer *pPlayer = itr->second;
		if (pPlayer)
		{
			if (pPlayer->m_dwAreaGroup == nFromServerID)
			{
				vecPlayer.push_back(pPlayer);
			}
		}
	}
	if (vecPlayer.size() > 0)
	{
		for (auto pPlayer : vecPlayer)
		{
			gpPlayerMapCtrl->KickPlayer(pPlayer);
		}
	}
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SINGLETON(CRedisPacketBuilder);

string CRedisPacketBuilder::BuildSendRedisData(WORD wCmdID, int nToServerId, const char* pData, int nDataLen)
{
	ReidsCmdHead stHead;
	stHead.nToServerId = nToServerId;
	stHead.nFromServerId = gpLoadConfig->m_wServerID;
	stHead.wCmdCode = wCmdID;
	stHead.nDataLen = nDataLen;

	string strPkg;
	strPkg.append((char*)&stHead, sizeof(ReidsCmdHead));
	strPkg.append(pData, nDataLen);
	return strPkg;
}

//////////////////////////////////////////////////////////////////////

string CRedisPacketBuilder::BuildSendEmail(int nServerId, UINT64 qwPlayerId, string &strTitle, string &strContent, std::vector<EmailItem> &listPrize)
{
	TRY_START
	RedisCmd::EmailInfo  emailInfo;

	emailInfo.set_playerid(qwPlayerId);
	emailInfo.set_title(strTitle);
	emailInfo.set_content(strContent);

	string strPrize;
	char szMsg[200];

	std::vector<EmailItem>::iterator itr;
	for (itr = listPrize.begin(); itr != listPrize.end();itr++)
	{
		RedisCmd::PrizeInfo *pInfo = emailInfo.add_infos();
		if (pInfo)
		{
			pInfo->set_itemtype((*itr).itemType);
			pInfo->set_itemidx((*itr).itemIdx);
			pInfo->set_itemnum((*itr).itemNum);
			pInfo->set_itembind((*itr).bind);

			snprintf(szMsg, sizeof(szMsg), "%d,%d,%d,%d;", (*itr).itemType, (*itr).itemIdx, (*itr).itemNum, (*itr).bind);
			strPrize += szMsg;
		}

	}

	CLog::ErrorLog("send mail:%d|%llu|%s", nServerId, qwPlayerId, strPrize.c_str());
	
	if (emailInfo.SerializeToArray(m_szPacketBuff, MAX_SEND_REDIS_PACKET_LEN))
	{
		return BuildSendRedisData(R_SEND_EMAIL, nServerId, m_szPacketBuff, emailInfo.ByteSize());
	}

	TRY_STOP
	return "";
}

string CRedisPacketBuilder::BuildSendKuaFuInfoReq(int nServerId, int nKFType, CPlayer *pPlayer, string strMapCode, string &strPlayerData)
{
	RedisCmd::KuaFuInfoReq  kuafuInfo;

	kuafuInfo.set_playerid(pPlayer->m_PlayerID);
	kuafuInfo.set_ptid(pPlayer->m_strPTID);
	if (nKFType == E_KUAFUTYPE_SUBAK)
	{
		kuafuInfo.set_guildname((pPlayer->m_pGuild) ? pPlayer->m_pGuild->m_strGuildName : "");
	}
	
	kuafuInfo.set_areagroup(pPlayer->m_dwAreaGroup);
	kuafuInfo.set_playername(pPlayer->m_strName);
	kuafuInfo.set_areaname(pPlayer->m_strAreaName);

	kuafuInfo.set_playerlevel(pPlayer->GetObjProperty(Attr_Level).Valint);
	kuafuInfo.set_sex(pPlayer->GetObjProperty(Attr_Sex).Valint);
	kuafuInfo.set_job(pPlayer->GetObjProperty(Attr_Job).Valint);
	kuafuInfo.set_duty(pPlayer->GetObjProperty(Attr_GuildDuty).Valint);

	kuafuInfo.set_mapcode(strMapCode);
	kuafuInfo.set_playerdata(strPlayerData);
	kuafuInfo.set_jumptype(nKFType);
	kuafuInfo.set_merareagroup(gpLoadConfig->m_wServerID);

	if (pPlayer->isHero())
	{
		kuafuInfo.set_roletype(1);
		kuafuInfo.set_masterplayer(pPlayer->m_pMasterPlayer->m_PlayerID);
	}
	

	if (kuafuInfo.SerializeToArray(m_szPacketBuff, MAX_SEND_REDIS_PACKET_LEN))
	{
		return BuildSendRedisData(R_KUAFU_REQ, nServerId, m_szPacketBuff, kuafuInfo.ByteSize());
	}
	return "";
}

string CRedisPacketBuilder::BuildSendKuaFuInfoAck(int nServerId, int nKFType, UINT64 qwPlayerId, int nRet, string strJumpMapCode)
{
	RedisCmd::KuaFuInfoAck  kuafuInfoack;

	kuafuInfoack.set_playerid(qwPlayerId);
	kuafuInfoack.set_ret(nRet);
	kuafuInfoack.set_jumptype(nKFType);
	kuafuInfoack.set_mapcode(strJumpMapCode);

	if (kuafuInfoack.SerializeToArray(m_szPacketBuff, MAX_SEND_REDIS_PACKET_LEN))
	{
		return BuildSendRedisData(R_KUAFU_ACK, nServerId, m_szPacketBuff, kuafuInfoack.ByteSize());
	}
	return "";
}

string CRedisPacketBuilder::BuildSendKuaFuKickPlayer(int nServerId, UINT64 qwPlayerId, string strPtID)
{
	RedisCmd::KuaFuKickPlayer  kuafukickplayer;

	kuafukickplayer.set_playerid(qwPlayerId);
	kuafukickplayer.set_ptid(strPtID);

	if (kuafukickplayer.SerializeToArray(m_szPacketBuff, MAX_SEND_REDIS_PACKET_LEN))
	{
		return BuildSendRedisData(R_KUAFU_KICK_PLAYER, nServerId, m_szPacketBuff, kuafukickplayer.ByteSize());
	}
	return "";
}

string CRedisPacketBuilder::BuildSendKuaFuSubYB(int nServerId, UINT64 qwPlayerId, int nCount)
{
	RedisCmd::KuaFuSubYB  kuafusubyb;

	kuafusubyb.set_playerid(qwPlayerId);
	kuafusubyb.set_count(nCount);

	if (kuafusubyb.SerializeToArray(m_szPacketBuff, MAX_SEND_REDIS_PACKET_LEN))
	{
		return BuildSendRedisData(R_KUAFU_SUB_YB, nServerId, m_szPacketBuff, kuafusubyb.ByteSize());
	}
	return "";
}


string CRedisPacketBuilder::BuildSendSubKing(int nServerId, std::string sGuildName)
{
	RedisCmd::KFSubKing kfSubKing;

	kfSubKing.set_guildname(sGuildName);

	if (kfSubKing.SerializeToArray(m_szPacketBuff, MAX_SEND_REDIS_PACKET_LEN))
	{
		return BuildSendRedisData(R_KUAFU_SUB_KING_GUILD, nServerId, m_szPacketBuff, kfSubKing.ByteSize());
	}
	return "";
}

string CRedisPacketBuilder::BuildSendMonitorServerStatus(int nStatus)
{
	RedisCmd::SeverReport  serverreport;

	serverreport.set_serverid(gpLoadConfig->m_wServerID);
	serverreport.set_onlinenum(gpPlayerMapCtrl->m_insPlayerMap.size());
	serverreport.set_status(nStatus);

	if (gpActivity)
	{
		serverreport.set_opentime(gpActivity->m_OpenServerTimeInfo.strServerOpenTime);
		serverreport.set_serverversion(GAME_VERSION);
	}
	

	if (serverreport.SerializeToArray(m_szPacketBuff, MAX_SEND_REDIS_PACKET_LEN))
	{
		return BuildSendRedisData(R_SEND_REGISTER, gpLoadConfig->m_wServerID, m_szPacketBuff, serverreport.ByteSize());
	}
	return "";
}

string CRedisPacketBuilder::BuildSendMonitorServerErrorMsg(int nType, string strMsg)
{
	RedisCmd::ServerError  servererror;

	servererror.set_serverid(gpLoadConfig->m_wServerID);
	servererror.set_errorcode(nType);
	servererror.set_errormsg(strMsg);

	if (servererror.SerializeToArray(m_szPacketBuff, MAX_SEND_REDIS_PACKET_LEN))
	{
		return BuildSendRedisData(R_SEND_ERROR, gpLoadConfig->m_wServerID, m_szPacketBuff, servererror.ByteSize());
	}
	return "";
}

// 广播
string CRedisPacketBuilder::BuildSendBroadcast(int nServerId, std::string newsId)
{
	RedisCmd::KuaFuBroadcast  info;

	info.set_newsid(newsId);

	if (info.SerializeToArray(m_szPacketBuff, MAX_SEND_REDIS_PACKET_LEN))
	{
		return BuildSendRedisData(R_SEND_BROADCAST, nServerId, m_szPacketBuff, info.ByteSize());
	}
	return "";
}

string CRedisPacketBuilder::BuildSendOffLine(int nServerId)
{
	return BuildSendRedisData(R_KUAFU_OFFLINE, nServerId, "", 0);
}

std::string CRedisPacketBuilder::CrossContent(int nServerId ,std::string strContent)
{
	RedisCmd::KFCrossContent kfContent;
	kfContent.set_crosscontent(strContent);

	if (kfContent.SerializeToArray(m_szPacketBuff, MAX_SEND_REDIS_PACKET_LEN))
	{
		return BuildSendRedisData(R_KUAFU_CONTENT, nServerId, m_szPacketBuff, kfContent.ByteSize());
	}

	return "";
}

std::string CRedisPacketBuilder::KFYuanBaoConsume(int nServerId, UINT64 qwPlayerId, std::string strOrderId, DWORD dwCount ,int nOperation)
{
	RedisCmd::KFYuanBaoConsume  kfyuanbao; 
	kfyuanbao.set_playerid(qwPlayerId);
	kfyuanbao.set_strorder(strOrderId);
	kfyuanbao.set_kfyuanbaocount(dwCount);
	kfyuanbao.set_kfoperation(nOperation);

	if (kfyuanbao.SerializeToArray(m_szPacketBuff, MAX_SEND_REDIS_PACKET_LEN))
	{
		return BuildSendRedisData(R_KUAFU_YUANBAO, nServerId, m_szPacketBuff, kfyuanbao.ByteSize());
	}

	return "";
}


std::string CRedisPacketBuilder::KFYuanBaoResult(int nServerId, UINT64 qwPlayerId, std::string strOrderId, DWORD dwResult ,int nOperation)
{
	RedisCmd::KFYuanBaoResult result;

	result.set_playerid(qwPlayerId);
	result.set_strorder(strOrderId);
	result.set_result(dwResult);
	result.set_kfoperation(nOperation);

	if (result.SerializeToArray(m_szPacketBuff, MAX_SEND_REDIS_PACKET_LEN))
	{
		return BuildSendRedisData(R_KUAFU_YUANBAO_RESULT, nServerId, m_szPacketBuff, result.ByteSize());
	}
	
	return "";
}

std::string CRedisPacketBuilder::KFYuanBaoRollBack(int nServerId, UINT64 u64PlayerId, std::string strOrderId, DWORD dwCount, int nIsRollBack)
{
	RedisCmd::KFYuanBaoConsume consume ;
	consume.set_playerid(u64PlayerId);
	consume.set_strorder(strOrderId);
	consume.set_kfyuanbaocount(dwCount);
	consume.set_isrollback(nIsRollBack);
	
	if (consume.SerializePartialToArray(m_szPacketBuff, MAX_SEND_REDIS_PACKET_LEN))
	{
		return BuildSendRedisData(R_KUAFU_YUANBAO_ROLLBACK, nServerId, m_szPacketBuff, consume.ByteSize());
	}

	return "";
}
