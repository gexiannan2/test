
#if !defined(AFX_REDISPROTOCOL_H_INCLUDED_)
#define AFX_REDISPROTOCOL_H_INCLUDED_

#include "stdafx.h"
#include "../Game/Player.h"
#include "../Game/Email.h"

#pragma pack(push, 1)

//[GS间协议修改] GS间协议的包头
typedef struct tagReidsCmdHead
{
	WORD wCmdCode;    //协议号
	int nToServerId;  //接收的服务器编号
	int nFromServerId;  //发送的服务器编号
	int nDataLen;	  //数据包长度
}ReidsCmdHead;

#pragma pack(pop)

//////////////////////////
enum KuaFuInfoAckType
{
	ES_KUAFU_ALLOW = 1,	//允许跨服
	ES_KUAFU_OFF = 2,		//数据传输失败
	ES_KUAFU_FULL = 3,	//跨服人数满
};

//跨服活动
enum EKuaFuJumpType
{
	E_KUAFUTYPE_SUBAK = 1,	//跨服沙巴克战
	E_KUAFUTYPE_TOWER = 2,  //跨服爬塔
	E_KUAFUTYPE_CHAOS = 3,  //跨服乱斗
};

struct MyKuaFuPlayer
{
	DWORD dwTick;
	int  nFromServer;
	UINT64 qwPlayerid;
	int nKFType;
	string strJumpMap;
};

class CRedisSession;
class CRedisParsers
{
	DECLARE_SINGLETON(CRedisParsers);
public:
	map<string, MyKuaFuPlayer> m_KuaFuPlayer;
	void ProcessPacket(CRedisSession* pSubSession, const char *pData, int nLen);//处理接收到的数据包
private:
	
	//收到发送邮件
	void OnRecvSendEmail(int nFromServerID, const char *pData, int nLen);
	//收到跨服请求数据
	void OnRecvKuaFuInfo(int nFromServerID, const char *pData, int nLen);
	//收到跨服回应
	void OnRecvKuaFuInfoAck(int nFromServerID, const char *pData, int nLen);
	//收到跨服踢人请求
	void OnRecvKuaFuKickPlayer(int nFromServerID, const char *pData, int nLen);
	//收到跨服消耗元宝请求
	//void OnRecvKuaFuSubYB(int nFromServerID, const char *pData, int nLen);
	//广播（播报）
	void OnRecvSendBroadcast(int nFromServerID, const char *pData, int nLen);
	//收到原服务器掉线通知踢掉玩家
	void OnRecvOffLine(int nFromServerID);
	// 跨服沙城得主
	void BuildSendKFSubKing(int nFromServerID, const char *pData, int nLen);

	void OnRecvKuaFuContent(int nFromServerID, const char *pData, int nLen);

	void OnRecvKuaFuYuanBao(int nFromServerID, const char *pData, int nLen);

	void OnRecvKuaFuYuanBaoResult(int nFromServerID, const char * pData, int nLen);

	void OnRecvKuaFuYuanBaoRollBack(int nFromServerID, const char * pData, int nLen);
};
#define gpRedisParsers    (CRedisParsers::Instance())

//////////////////////////////////////////////////////////////////////////
#define MAX_SEND_REDIS_PACKET_LEN 100*1024

class CRedisPacketBuilder
{
	DECLARE_SINGLETON(CRedisPacketBuilder);

private:
	string BuildSendRedisData(WORD wCmdID,int nToServerId,const char* pData,int nDataLen);

	char m_szPacketBuff[MAX_SEND_REDIS_PACKET_LEN];
public:

	//收到发送邮件
	string BuildSendEmail(int nServerId, UINT64 qwPlayerId, string &strTitle, string &strContent, std::vector<EmailItem> &listPrize);
	//发送跨服请求
	string BuildSendKuaFuInfoReq(int nServerId, int nKFType, CPlayer *pPlayer, string strMapCode, string &strPlayerData);
	//回复跨服请求
	string BuildSendKuaFuInfoAck(int nServerId, int nKFType, UINT64 qwPlayerId, int nRet, string strJumpMapCode="");
	//回复跨服请求
	string BuildSendKuaFuKickPlayer(int nServerId, UINT64 qwPlayerId, string strPtID);
	//跨服发送元宝消耗
	string BuildSendKuaFuSubYB(int nServerId, UINT64 qwPlayerId, int nCount);
	//发送服务器状态及人数
	string BuildSendMonitorServerStatus(int nStatus);
	//发送报错信息
	string BuildSendMonitorServerErrorMsg(int nType, string strMsg);
	// 广播
	string BuildSendBroadcast(int nServerId, std::string newsId);
	//发送原服务器掉线通知踢掉玩家
	string BuildSendOffLine(int nServerId);

	// 
	string BuildSendSubKing(int nServerId, std::string sGuildName);

	std::string CrossContent(int nServerId ,std::string strContent);

	std::string KFYuanBaoConsume(int nServerId, UINT64 qwPlayerId, std::string strOrderId, DWORD dwCount , int nOperationType);  //发到本服

	std::string KFYuanBaoResult(int nServerId, UINT64 qwPlayerId, std::string strOrderId,DWORD dwResult ,int nOperation) ;

	std::string KFYuanBaoRollBack(int nServerId, UINT64 u64PlayerId, std::string strOrderId, DWORD dwCount , int nIsRollBack);

};
#define gpRedisPacketBuilder    (CRedisPacketBuilder::Instance())


#endif 
