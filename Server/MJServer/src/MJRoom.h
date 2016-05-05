#pragma once
#include "ISitableRoom.h"
#include <json/json.h>
#include "MJDefine.h"
#include "MJCard.h"
class CMJRoomPlayer ;
class CMJRoom
	:public ISitableRoom
{
public:
	CMJRoom();
	bool onFirstBeCreated(IRoomManager* pRoomMgr,stBaseRoomConfig* pConfig, uint32_t nRoomID , Json::Value& vJsValue)override;
	void serializationFromDB(IRoomManager* pRoomMgr,stBaseRoomConfig* pConfig,uint32_t nRoomID , Json::Value& vJsValue )override;
	void willSerializtionToDB(Json::Value& vOutJsValue)override;
	void roomItemDetailVisitor(Json::Value& vOutJsValue)override;
	void prepareState()override ;
	void roomInfoVisitor(Json::Value& vOutJsValue)override ;
	void sendRoomPlayersInfo(uint32_t nSessionID)override ;
	void setBankerIdx(uint8_t nIdx ){ m_nBankerIdx = nIdx ;}
	uint8_t getBankerIdx(){ return m_nBankerIdx ;}
	uint32_t getBaseBet(); // ji chu di zhu ;
	
	void onGameWillBegin()override ;
	void onGameDidEnd()override ;
	void onPlayerWillStandUp( ISitableRoomPlayer* pPlayer )override ;
	uint32_t getLeastCoinNeedForCurrentGameRound(ISitableRoomPlayer* pp)override ;
	uint8_t getRoomType()override{ return eRoom_MJ ;}
	void prepareCards()override;
	uint32_t coinNeededToSitDown()override;
	void caculateGameResult();
	void onPlayerHuPai(uint8_t nActIdx);
	void onPlayerHuPai(uint8_t nActIdx , uint8_t nCardNumber, uint8_t nInvokerIdx ,bool isGangPai );
	void onPlayerGangPai( uint8_t nActIdx ,uint8_t nCardNumber, bool isBuGang , uint8_t nInvokeIdx );
	bool checkPlayersNeedTheCard( uint8_t nCardNumber ,std::vector<uint8_t>& nNeedCardPlayerIdxs, uint8_t nExptPlayerIdx );
	void onPlayerBuGangPre(uint8_t nPlayerIdx , uint8_t nCardNumber );
	void onPlayerChuPai(uint8_t nPlayerIdx , uint8_t nCardNumber );
	uint8_t getLeftCardCnt();
	uint8_t getNextActPlayerIdx( uint8_t nCurActIdx );
	void onPlayerMoPai( uint8_t nIdx );
	void onPlayerPeng(uint8_t nPlayerIdx ,uint8_t nCardNumber );
	bool canPlayerGangWithCard(uint8_t nPlayerIdx , uint8_t nCardNumber, bool bCardFromSelf );
	bool canPlayerHuPai( uint8_t nPlayerIdx , uint8_t nCardNumber );
	bool canPlayerPengPai(uint8_t nPlayerIdx , uint8_t nCardNumber);
	uint8_t getPlayerAutoChuCardWhenTimeOut(uint8_t nPlayerIdx);
	void onPlayerRallBackWindRain(CMJRoomPlayer* pPlayer );
protected:
	uint32_t getCacualteCoin( uint8_t nFanshu , uint8_t nGenShu );
	ISitableRoomPlayer* doCreateSitableRoomPlayer() override;
protected:
	uint8_t m_nBankerIdx ;
	uint32_t m_nBaseBet ;

	CMJCard m_tPoker ;
	Json::Value m_arrPlayers ;
};