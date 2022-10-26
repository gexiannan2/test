
#if !defined(AFX_REDISPROTOCOL_H_INCLUDED_)
#define AFX_REDISPROTOCOL_H_INCLUDED_

#include "stdafx.h"
#include "../Game/Player.h"
#include "../Game/Email.h"

#pragma pack(push, 1)

//[GS��Э���޸�] GS��Э��İ�ͷ
typedef struct tagReidsCmdHead
{
	WORD wCmdCode;    //Э���
	int nToServerId;  //���յķ��������
	int nFromServerId;  //���͵ķ��������
	int nDataLen;	  //���ݰ�����
}ReidsCmdHead;

#pragma pack(pop)

//////////////////////////
enum KuaFuInfoAckType
{
	ES_KUAFU_ALLOW = 1,	//������
	ES_KUAFU_OFF = 2,		//���ݴ���ʧ��
	ES_KUAFU_FULL = 3,	//���������
};

//����
enum EKuaFuJumpType
{
	E_KUAFUTYPE_SUBAK = 1,	//���ɳ�Ϳ�ս
	E_KUAFUTYPE_TOWER = 2,  //�������
	E_KUAFUTYPE_CHAOS = 3,  //����Ҷ�
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
	void ProcessPacket(CRedisSession* pSubSession, const char *pData, int nLen);//������յ������ݰ�
private:
	
	//�յ������ʼ�
	void OnRecvSendEmail(int nFromServerID, const char *pData, int nLen);
	//�յ������������
	void OnRecvKuaFuInfo(int nFromServerID, const char *pData, int nLen);
	//�յ������Ӧ
	void OnRecvKuaFuInfoAck(int nFromServerID, const char *pData, int nLen);
	//�յ������������
	void OnRecvKuaFuKickPlayer(int nFromServerID, const char *pData, int nLen);
	//�յ��������Ԫ������
	//void OnRecvKuaFuSubYB(int nFromServerID, const char *pData, int nLen);
	//�㲥��������
	void OnRecvSendBroadcast(int nFromServerID, const char *pData, int nLen);
	//�յ�ԭ����������֪ͨ�ߵ����
	void OnRecvOffLine(int nFromServerID);
	// ���ɳ�ǵ���
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

	//�յ������ʼ�
	string BuildSendEmail(int nServerId, UINT64 qwPlayerId, string &strTitle, string &strContent, std::vector<EmailItem> &listPrize);
	//���Ϳ������
	string BuildSendKuaFuInfoReq(int nServerId, int nKFType, CPlayer *pPlayer, string strMapCode, string &strPlayerData);
	//�ظ��������
	string BuildSendKuaFuInfoAck(int nServerId, int nKFType, UINT64 qwPlayerId, int nRet, string strJumpMapCode="");
	//�ظ��������
	string BuildSendKuaFuKickPlayer(int nServerId, UINT64 qwPlayerId, string strPtID);
	//�������Ԫ������
	string BuildSendKuaFuSubYB(int nServerId, UINT64 qwPlayerId, int nCount);
	//���ͷ�����״̬������
	string BuildSendMonitorServerStatus(int nStatus);
	//���ͱ�����Ϣ
	string BuildSendMonitorServerErrorMsg(int nType, string strMsg);
	// �㲥
	string BuildSendBroadcast(int nServerId, std::string newsId);
	//����ԭ����������֪ͨ�ߵ����
	string BuildSendOffLine(int nServerId);

	// 
	string BuildSendSubKing(int nServerId, std::string sGuildName);

	std::string CrossContent(int nServerId ,std::string strContent);

	std::string KFYuanBaoConsume(int nServerId, UINT64 qwPlayerId, std::string strOrderId, DWORD dwCount , int nOperationType);  //��������

	std::string KFYuanBaoResult(int nServerId, UINT64 qwPlayerId, std::string strOrderId,DWORD dwResult ,int nOperation) ;

	std::string KFYuanBaoRollBack(int nServerId, UINT64 u64PlayerId, std::string strOrderId, DWORD dwCount , int nIsRollBack);

};
#define gpRedisPacketBuilder    (CRedisPacketBuilder::Instance())


#endif 
