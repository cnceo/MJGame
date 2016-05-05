#include "PlayerBaseData.h"
#include <string>
#include "MessageDefine.h"
#include "ServerMessageDefine.h"
#include "Player.h"
#include "LogManager.h"
#include <time.h>
#include "GameServerApp.h"
#include "ContinueLoginConfig.h"
#include "PlayerItem.h"
#include "PlayerEvent.h"
#include "PlayerManager.h"
#include "EventCenter.h"
#include "InformConfig.h"
#include "AutoBuffer.h"
#include "PlayerManager.h"
#include "ShopConfg.h"
#include "RewardConfig.h"
#include "PlayerGameData.h"
#include "PlayerMail.h"
#include <assert.h>
#include "SeverUtility.h"
#include "ServerStringTable.h"
#pragma warning( disable : 4996 )
#define ONLINE_BOX_RESET_TIME 60*60*3   // offline 3 hour , will reset the online box ;
#define COIN_BE_INVITED 588
#define COIN_INVITE_PRIZE 3000
#define  COIN_FOR_VIP_CARD 6800
CPlayerBaseData::CPlayerBaseData(CPlayer* player )
	:IPlayerComponent(player)
{
	m_eType = ePlayerComponent_BaseData ;
	memset(&m_stBaseData,0,sizeof(m_stBaseData)) ;
	m_bGivedLoginReward = false ;
	m_strCurIP = "" ;
	m_nTempCoin = 0 ;
	setTempCoin(0);
}

CPlayerBaseData::~CPlayerBaseData()
{

}

void CPlayerBaseData::Init()
{
	memset(&m_stBaseData,0,sizeof(m_stBaseData)) ;
	m_stBaseData.nUserUID = GetPlayer()->GetUserUID() ;
	m_nTempCoin = 0 ;
	m_bGivedLoginReward = false ;

	m_bMoneyDataDirty = false;
	m_bCommonLogicDataDirty = false;
	m_bPlayerInfoDataDirty = false;
	m_ePlayerType = ePlayer_Normal ;

	m_strCurIP = "" ;
	nReadingDataFromDB = 0 ;
	Reset();
}

void CPlayerBaseData::Reset()
{
	m_ePlayerType = ePlayer_Normal ;
	m_nTempCoin = 0 ;
	setTempCoin(0);
	m_strCurIP = "" ;
	m_bGivedLoginReward = false ;
	nReadingDataFromDB = 0 ;

	m_bMoneyDataDirty = false;
	m_bCommonLogicDataDirty = false;
	m_bPlayerInfoDataDirty = false;

	memset(&m_stBaseData,0,sizeof(m_stBaseData)) ;
	m_stBaseData.nUserUID = GetPlayer()->GetUserUID() ;

	stMsgDataServerGetBaseData msg ;
	msg.nUserUID = GetPlayer()->GetUserUID() ;
	SendMsg(&msg,sizeof(msg)) ;
	nReadingDataFromDB = 1 ;
	CLogMgr::SharedLogMgr()->PrintLog("requesting userdata for uid = %d",msg.nUserUID);
	// register new day event ;
	CEventCenter::SharedEventCenter()->RegisterEventListenner(eEvent_NewDay,this,CPlayerBaseData::EventFunc ) ;

	stMsgRequestClientIp msgReq ;
	SendMsg(&msgReq,sizeof(msgReq)) ;
	CLogMgr::SharedLogMgr()->PrintLog("send request ip , sessioni id = %d",GetPlayer()->GetSessionID()) ;
}

void CPlayerBaseData::onBeInviteBy(uint32_t nInviteUID )
{
	if ( m_stBaseData.nInviteUID )
	{
		CLogMgr::SharedLogMgr()->PrintLog("can not do twice be invited ") ;
		return ;
	}
	m_stBaseData.nInviteUID = nInviteUID ;
	m_bPlayerInfoDataDirty = true ;

	// give self prize ;
	AddMoney(COIN_BE_INVITED) ;

	// show dlg ;
	stMsgDlgNotice msg;
	msg.nNoticeType = eNotice_BeInvite ;
	Json::Value jNotice ;
	jNotice["targetUID"] = nInviteUID ;
	jNotice["addCoin"] = COIN_BE_INVITED ;
	Json::StyledWriter writer ;
	std::string strNotice = writer.write(jNotice) ;
	msg.nJsonLen = strNotice.size();
	CAutoBuffer msgBuffer(sizeof(msg) + msg.nJsonLen );
	msgBuffer.addContent(&msg,sizeof(msg)) ;
	msgBuffer.addContent(strNotice.c_str(),msg.nJsonLen) ;
	SendMsg((stMsg*)msgBuffer.getBufferPtr(),msgBuffer.getContentSize()) ;
	CLogMgr::SharedLogMgr()->PrintLog("uid = %d be invite give prize coin = %d",GetPlayer()->GetUserUID(),COIN_BE_INVITED) ;

	// give prize to inviter ;
	auto player = CGameServerApp::SharedGameServerApp()->GetPlayerMgr()->GetPlayerByUserUID(nInviteUID) ;
	if ( player )
	{
		player->GetBaseData()->addInvitePrize(COIN_INVITE_PRIZE);

		//stMsgDlgNotice msg;
		//msg.nNoticeType = eNotice_InvitePrize ;
		//Json::StyledWriter writer ;
		//std::string strNotice = writer.write(jNotice) ;
		//msg.nJsonLen = strNotice.size();
		//CAutoBuffer msgBuffer(sizeof(msg) + msg.nJsonLen );
		//msgBuffer.addContent(&msg,sizeof(msg)) ;
		//msgBuffer.addContent(strNotice.c_str(),msg.nJsonLen) ;
		//player->GetBaseData()->SendMsg((stMsg*)msgBuffer.getBufferPtr(),msgBuffer.getContentSize()) ;
		CLogMgr::SharedLogMgr()->PrintLog("invite id = %d online just give prize ",nInviteUID) ;
	}
	else
	{
		Json::Value jconArg;
		jconArg["comment"] = "invitePrize" ;
		jconArg["addCoin"] = COIN_INVITE_PRIZE ;
		CPlayerMailComponent::PostOfflineEvent(CPlayerMailComponent::Event_AddCoin,jconArg,nInviteUID) ;
		CLogMgr::SharedLogMgr()->PrintLog("invite id = %d not online just post a mail",nInviteUID) ;
	}

	// send a mail to inviter 
	jNotice["targetUID"] = GetPlayer()->GetUserUID() ;
	jNotice["addCoin"] = COIN_INVITE_PRIZE ;
	Json::StyledWriter writerInfo ;
	strNotice = writerInfo.write(jNotice) ;
	CPlayerMailComponent::PostMailToPlayer(eMail_InvitePrize,strNotice.c_str(),strNotice.size(),nInviteUID);

	// send push notification ;
	CSendPushNotification::getInstance()->reset();
	CSendPushNotification::getInstance()->addTarget(nInviteUID) ;
	CSendPushNotification::getInstance()->setContent(CServerStringTable::getInstance()->getStringByID(5),1) ;
	auto abf = CSendPushNotification::getInstance()->getNoticeMsgBuffer();
	SendMsg((stMsg*)abf->getBufferPtr(),abf->getContentSize()) ;
}

bool CPlayerBaseData::OnMessage( stMsg* pMsg , eMsgPort eSenderPort )
{
	if ( IPlayerComponent::OnMessage(pMsg,eSenderPort) )
	{
		return true ;
	}

	switch( pMsg->usMsgType )
	{
	case MSG_REQ_TOTAL_GAME_OFFSET:
		{
			stMsgReqRobotTotalGameOffsetRet msgBack ;
			msgBack.nTotalGameOffset = m_stBaseData.nTotalGameCoinOffset ;
			SendMsg(&msgBack,sizeof(msgBack)) ;
			CLogMgr::SharedLogMgr()->PrintLog("robot uid = %u req total offset = %d",GetPlayer()->GetUserUID(),msgBack.nTotalGameOffset) ;
 		}
		break;
	case MSG_TELL_PLAYER_TYPE:
		{
			stMsgTellPlayerType* pRet = (stMsgTellPlayerType*)pMsg ;
			m_ePlayerType = (ePlayerType)pRet->nPlayerType ;
			CLogMgr::SharedLogMgr()->PrintLog("uid = %u , tell player type = %u",GetPlayer()->GetUserUID(),m_ePlayerType);
		}
		break;
	case MSG_GET_VIP_CARD_GIFT:
		{
			stMsgGetVipcardGift* pRet = (stMsgGetVipcardGift*)pMsg ;
			stMsgGetVipcardGiftRet msgBack ;
			msgBack.nVipCardType = pRet->nVipCardType ;
			msgBack.nAddCoin = 0 ;

			if ( m_stBaseData.nCardType != pRet->nVipCardType )
			{
				msgBack.nRet = 1 ;
				SendMsg(&msgBack,sizeof(msgBack)) ;
				CLogMgr::SharedLogMgr()->PrintLog("player uid = %d get vip card coin ret = %d",GetPlayer()->GetUserUID(),msgBack.nRet) ;
				break; 
			}

			time_t tNow = time(nullptr) ;
			if ( m_stBaseData.nCardEndTime < tNow )
			{
				msgBack.nRet = 2 ;
				SendMsg(&msgBack,sizeof(msgBack)) ;
				CLogMgr::SharedLogMgr()->PrintLog("player uid = %d get vip card coin ret = %d",GetPlayer()->GetUserUID(),msgBack.nRet) ;
				break; 
			}

			struct tm pTempNow,pLastTake ;
			pTempNow = *localtime(&tNow) ;
			time_t nLastTake = m_stBaseData.tLastTakeCardGiftTime ;
			pLastTake = *localtime(&nLastTake) ;
			if ( pTempNow.tm_year == pLastTake.tm_year && pTempNow.tm_mon == pLastTake.tm_mon && pTempNow.tm_yday == pLastTake.tm_yday )
			{
				msgBack.nRet = 3 ;
				SendMsg(&msgBack,sizeof(msgBack)) ;
				CLogMgr::SharedLogMgr()->PrintLog("player uid = %d get vip card coin ret = %d",GetPlayer()->GetUserUID(),msgBack.nRet) ;
				break; 
			}

			msgBack.nRet = 0 ;
			msgBack.nAddCoin = COIN_FOR_VIP_CARD ;
			AddMoney(COIN_FOR_VIP_CARD);
			m_stBaseData.tLastTakeCardGiftTime = tNow ;
			SendMsg(&msgBack,sizeof(msgBack)) ;
			m_bCommonLogicDataDirty = true ;
			CLogMgr::SharedLogMgr()->PrintLog("player uid = %d get vip card coin ret = %d",GetPlayer()->GetUserUID(),msgBack.nRet) ;
		}
		break;
	case MSG_PLAYER_CHECK_INVITER:
		{
			stMsgCheckInviterRet msgBack ;
			stMsgCheckInviter* pRet = (stMsgCheckInviter*)pMsg ;
			msgBack.nInviterUID = pRet->nInviterUID ;

			if ( m_stBaseData.nUserUID == pRet->nInviterUID )
			{
				msgBack.nRet = 1 ;
				CLogMgr::SharedLogMgr()->ErrorLog("can not invite self , uid = %d",GetPlayer()->GetUserUID()) ;
				SendMsg(&msgBack,sizeof(msgBack)) ;
				break; 
			}

			if ( m_stBaseData.nInviteUID )
			{
				msgBack.nRet = 2 ;
				CLogMgr::SharedLogMgr()->PrintLog("you already have invite = %d , uid = %d",m_stBaseData.nInviteUID,GetPlayer()->GetUserUID()) ;
				SendMsg(&msgBack,sizeof(msgBack)) ; 
				break; 
			}

			auto tPlayer = CGameServerApp::SharedGameServerApp()->GetPlayerMgr()->GetPlayerByUserUID(pRet->nInviterUID);
			if ( tPlayer )
			{
				CLogMgr::SharedLogMgr()->PrintLog("do invite = %d ,player = %d",pRet->nInviterUID,GetPlayer()->GetUserUID()) ;
				onBeInviteBy(pRet->nInviterUID) ;
				msgBack.nRet = 0 ;
				SendMsg(&msgBack,sizeof(msgBack)) ;
			}
			else
			{
				stMsgDBCheckInvite msgCheck ;
				msgCheck.nInviteUserUID = pRet->nInviterUID ;
				SendMsg(&msgCheck,sizeof(msgCheck)) ;
				CLogMgr::SharedLogMgr()->PrintLog("invite = %d not online, so ask db i am uid = %d",pRet->nInviterUID,GetPlayer()->GetUserUID()) ;
			}
		}
		break;
	case MSG_DB_CHECK_INVITER:
		{
			stMsgCheckInviterRet msgBack ;
			stMsgDBCheckInviteRet* pRet = (stMsgDBCheckInviteRet*)pMsg ;
			msgBack.nInviterUID = pRet->nInviteUseUID ;
			if ( pRet->nRet )
			{
				msgBack.nRet = 1 ;
				SendMsg(&msgBack,sizeof(msgBack)) ;
			}
			else
			{
				onBeInviteBy(pRet->nInviteUseUID) ;
				msgBack.nRet = 0 ;
				SendMsg(&msgBack,sizeof(msgBack)) ;
			}
		}
		break;
	case MSG_REQUEST_CLIENT_IP:
		{
			stMsgRequestClientIpRet* pRet = (stMsgRequestClientIpRet*)pMsg ;
			if ( pRet->nRet == 0 )
			{
				m_strCurIP = pRet->vIP ;
				CLogMgr::SharedLogMgr()->PrintLog("get client ip = %s session id = %d",m_strCurIP.c_str(),GetPlayer()->GetSessionID()) ;
			}
			else
			{
				CLogMgr::SharedLogMgr()->ErrorLog("cant not request client ip , uid = %d",GetPlayer()->GetUserUID()) ;
			}
		}
		break;
	case MSG_SHOP_BUY_ITEM_ORDER:
		{
			stMsgPlayerShopBuyItemOrder* pRet = (stMsgPlayerShopBuyItemOrder*)pMsg ;
			stMsgPlayerShopBuyItemOrderRet msgBack ;
			msgBack.nChannel = pRet->nChannel ;
			msgBack.nShopItemID = pRet->nShopItemID ;
			msgBack.nRet = 0 ;
			memset(msgBack.cOutTradeNo,0,sizeof(msgBack.cOutTradeNo)) ;
			memset(msgBack.cPrepayId,0,sizeof(msgBack.cPrepayId)) ;
			CShopConfigMgr* pMgr = (CShopConfigMgr*)CGameServerApp::SharedGameServerApp()->GetConfigMgr()->GetConfig(CConfigManager::eConfig_Shop);
			stShopItem* pItem = pMgr->GetShopItem(pRet->nShopItemID);
			if ( pItem == nullptr )
			{
				msgBack.nRet = 1 ;
				SendMsg(&msgBack,sizeof(msgBack)) ;
				CLogMgr::SharedLogMgr()->ErrorLog("can not find shop item , for order uid = %d",GetPlayer()->GetUserUID()) ;
				break;
			}

			if ( msgBack.nChannel != ePay_WeChat )
			{
				msgBack.nRet = 4 ;
				SendMsg(&msgBack,sizeof(msgBack)) ;
				CLogMgr::SharedLogMgr()->ErrorLog("current must be wechat channel for order channel = %d, uid = %d",msgBack.nChannel,GetPlayer()->GetUserUID() );
				break;
			}

			if ( m_strCurIP.empty() )
			{
				msgBack.nRet = 2 ;
				SendMsg(&msgBack,sizeof(msgBack)) ;
				CLogMgr::SharedLogMgr()->ErrorLog("cur rent ip = null , for order uid = %d",GetPlayer()->GetUserUID()) ;
				break;
			}

			stMsgVerifyItemOrder msgOrder ;
			memset(msgOrder.cShopDesc,0,sizeof(msgOrder.cShopDesc));
			sprintf_s(msgOrder.cShopDesc,sizeof(msgOrder.cShopDesc),"%s",pItem->strItemName.c_str()) ;

			memset(msgOrder.cOutTradeNo,0,sizeof(msgOrder.cOutTradeNo));
			sprintf_s(msgOrder.cOutTradeNo,sizeof(msgOrder.cOutTradeNo),"%dE%dE%u",pItem->nShopItemID,GetPlayer()->GetUserUID(),(uint32_t)time(nullptr)) ;
			
			msgOrder.nPrize = pItem->nPrize * 100 ; 
			msgOrder.nChannel = pRet->nChannel ;

			memset(msgOrder.cTerminalIp,0,sizeof(msgOrder.cTerminalIp));
			sprintf_s(msgOrder.cTerminalIp,sizeof(msgOrder.cTerminalIp),"%s",m_strCurIP.c_str()) ;
			
			SendMsg(&msgOrder,sizeof(msgOrder)) ;
			CLogMgr::SharedLogMgr()->SystemLog("order shop item to verify shop item = %d , uid = %d",pItem->nShopItemID,GetPlayer()->GetUserUID()) ;
		} 
		break;
	case MSG_VERIFY_ITEM_ORDER:
		{
			stMsgVerifyItemOrderRet* pRet = (stMsgVerifyItemOrderRet*)pMsg ;
			stMsgPlayerShopBuyItemOrderRet msgBack ;
			memset(msgBack.cOutTradeNo,0,sizeof(msgBack.cOutTradeNo)) ;
			memset(msgBack.cPrepayId,0,sizeof(msgBack.cPrepayId)) ;
			msgBack.nChannel = pRet->nChannel ;
			
			std::string strTradeNo(pRet->cOutTradeNo,sizeof(pRet->cOutTradeNo));
			std::string shopItem = strTradeNo.substr(0,strTradeNo.find_first_of('E')) ;
			if ( shopItem.empty() )
			{
				msgBack.nShopItemID = 0 ;
				CLogMgr::SharedLogMgr()->ErrorLog("outTradeNo shop item is null , uid = %d",GetPlayer()->GetUserUID()) ;
			}
			else
			{
				msgBack.nShopItemID = atoi(shopItem.c_str()) ;
			}
			
			if ( pRet->nRet )
			{
				msgBack.nRet = 3 ;
			}
			else
			{
				msgBack.nRet = 0;
				memcpy(msgBack.cOutTradeNo,pRet->cOutTradeNo,sizeof(pRet->cOutTradeNo));
				memcpy(msgBack.cPrepayId,pRet->cPrepayId,sizeof(pRet->cPrepayId));
			}
			CLogMgr::SharedLogMgr()->SystemLog("shopitem id = %d shop order ret = %d, uid = %d",msgBack.nShopItemID,pRet->nRet,GetPlayer()->GetUserUID()) ;
			SendMsg(&msgBack,sizeof(msgBack)) ;
		}
		break;
	case MSG_BUY_SHOP_ITEM:
		{
			stMsgPlayerBuyShopItem* pRet = (stMsgPlayerBuyShopItem*)pMsg ;
			stMsgToVerifyServer msgVerify ;
			msgVerify.nBuyerPlayerUserUID = GetPlayer()->GetUserUID();
			msgVerify.nBuyForPlayerUserUID = msgVerify.nBuyerPlayerUserUID ;
			msgVerify.nMiUserUID = pRet->nMiUserUID ;
			msgVerify.nShopItemID = pRet->nShopItemID ;
			msgVerify.nTranscationIDLen = pRet->nBufLen ;
			msgVerify.nChannel = pRet->nChannelID ;
			CAutoBuffer buffer(sizeof(stMsgPlayerBuyShopItem) + pRet->nBufLen ) ;
			buffer.addContent(&msgVerify,sizeof(msgVerify));
			buffer.addContent(((char*)pRet) + sizeof(stMsgPlayerBuyShopItem),pRet->nBufLen);
			SendMsg((stMsg*)buffer.getBufferPtr(),buffer.getContentSize());
		}
		break;
	case MSG_VERIFY_TANSACTION:
		{
			stMsgFromVerifyServer* pRet = (stMsgFromVerifyServer*)pMsg ;
			stMsgPlayerBuyShopItemRet msgBack ;
			msgBack.nBuyShopItemForUserUID = pRet->nBuyForPlayerUserUID ;
			msgBack.nDiamoned = 0 ;
			msgBack.nSavedMoneyForVip = 0 ;
			msgBack.nShopItemID = pRet->nShopItemID ;
			msgBack.nRet = 0 ;
			if ( pRet->nRet == 4 ) // success 
			{
				if ( pRet->nShopItemID == 6 )
				{
					CLogMgr::SharedLogMgr()->SystemLog("uid = %d buy a vip card week card ",GetPlayer()->GetUserUID()) ;
					m_stBaseData.nCardEndTime = time(nullptr) + 60 * 60 * 24 * 8;
					m_stBaseData.nCardType = eCard_Week ;
					m_bCommonLogicDataDirty = true ;
				}
				else
				{
					CShopConfigMgr* pMgr = (CShopConfigMgr*)CGameServerApp::SharedGameServerApp()->GetConfigMgr()->GetConfig(CConfigManager::eConfig_Shop);
					stShopItem* pItem = pMgr->GetShopItem(pRet->nShopItemID);
					if ( pItem == nullptr )
					{
						msgBack.nRet = 5 ;
						CLogMgr::SharedLogMgr()->ErrorLog("can not find shop id = %d , buyer uid = %d",pRet->nShopItemID,pRet->nBuyerPlayerUserUID) ;
					}
					else
					{
						AddMoney(pItem->nCount) ;
						CLogMgr::SharedLogMgr()->SystemLog("add coin with shop id = %d for buyer uid = %d ",pRet->nShopItemID,pRet->nBuyerPlayerUserUID) ;
					}
				}

				// save log 
				stMsgSaveLog msgLog ;
				memset(msgLog.vArg,0,sizeof(msgLog.vArg));
				msgLog.nJsonExtnerLen = 0 ;
				msgLog.nLogType = eLog_Purchase ;
				msgLog.nTargetID = GetPlayer()->GetUserUID() ;
				memset(msgLog.vArg,0,sizeof(msgLog.vArg)) ;
				msgLog.vArg[0] = GetAllCoin() ;
				msgLog.vArg[1] = pRet->nShopItemID ;
				SendMsg(&msgLog,sizeof(msgLog)) ;
			}
			else
			{
				msgBack.nRet = 2 ;
				CLogMgr::SharedLogMgr()->ErrorLog("uid = %d ,shop id = %d , verify error ",pRet->nBuyerPlayerUserUID,pRet->nShopItemID) ;
			}

			msgBack.nFinalyCoin = GetAllCoin() ;
			SendMsg(&msgBack,sizeof(msgBack)) ;
		}
		break;
	case MSG_ON_PLAYER_BIND_ACCOUNT:
		{
			m_stBaseData.isRegister = true ;
			m_bPlayerInfoDataDirty = true ;
			CLogMgr::SharedLogMgr()->PrintLog("player bind account ok uid = %u",GetPlayer()->GetUserUID());
		}
		break;
	case MSG_READ_PLAYER_BASE_DATA:   // from db server ;
		{
			stMsgDataServerGetBaseDataRet* pBaseData = (stMsgDataServerGetBaseDataRet*)pMsg ;
			if ( pBaseData->nRet )
			{
				CLogMgr::SharedLogMgr()->ErrorLog("do not exsit playerData") ;
				return true; 
			}
			memcpy(&m_stBaseData,&pBaseData->stBaseData,sizeof(m_stBaseData));
			CLogMgr::SharedLogMgr()->PrintLog("recived base data uid = %d",pBaseData->stBaseData.nUserUID);
			nReadingDataFromDB = 2 ;
			SendBaseDatToClient();
			CGameServerApp::SharedGameServerApp()->GetPlayerMgr()->getPlayerDataCaher().removePlayerDataCache(pBaseData->stBaseData.nUserUID) ;
			return true ;
		}
		break;
//	case MSG_PLAYER_REQUEST_NOTICE:
//		{
//// 			CGameServerApp::SharedGameServerApp()->GetBrocaster()->SendInformsToPlayer(GetPlayer()) ;
//// 			CInformConfig* pConfig = (CInformConfig*)CGameServerApp::SharedGameServerApp()->GetConfigMgr()->GetConfig(CConfigManager::eConfig_Informs) ;
//// 			m_stBaseData.nNoticeID = pConfig->GetMaxInformID();
//		}
//		break;
	case MSG_PLAYER_MODIFY_SIGURE:
		{
			stMsgPLayerModifySigure* pMsgRet = (stMsgPLayerModifySigure*)pMsg ;
			memcpy(m_stBaseData.cSignature,pMsgRet->pNewSign,sizeof(m_stBaseData.cSignature));
			stMsgPlayerModifySigureRet ret ;
			ret.nRet = 0 ;
			SendMsg(&ret,sizeof(ret)) ;
			m_bPlayerInfoDataDirty = true ;
		}
		break;
	case MSG_PLAYER_MODIFY_NAME:
		{
			stMsgPLayerModifyName* pMsgRet = (stMsgPLayerModifyName*)pMsg ;
			stMsgPlayerModifyNameRet ret ;
			ret.nRet = 0 ;
			memcpy(ret.pName,pMsgRet->pNewName,sizeof(ret.pName));
			if ( pMsgRet->pNewName[sizeof(pMsgRet->pNewName) -1 ] != 0 )
			{
				ret.nRet = 1 ;
				CLogMgr::SharedLogMgr()->ErrorLog("name is too long uid = %d",GetPlayer()->GetUserUID());
			}
			else
			{
				memcpy(m_stBaseData.cName,pMsgRet->pNewName,sizeof(m_stBaseData.cName)) ;
				m_bPlayerInfoDataDirty = true ;
			}
			SendMsg(&ret,sizeof(ret)) ;
		}
		break;
	case MSG_PLAYER_MODIFY_PHOTO:
		{
			stMsgPlayerModifyPhoto* pPhoto = (stMsgPlayerModifyPhoto*)pMsg ;
			m_stBaseData.nPhotoID = pPhoto->nPhotoID ;
			stMsgPlayerModifyPhotoRet msgRet ;
			msgRet.nRet = 0 ;
			SendMsg(&msgRet,sizeof(msgRet)) ;
			m_bPlayerInfoDataDirty = true ;
		}
		break;
	case MSG_PLAYER_MODIFY_SEX:
		{
			stMsgPlayerModifySex* pRet = (stMsgPlayerModifySex*)pMsg ;
			m_stBaseData.nSex = pRet->nNewSex ;
			stMsgPlayerModifySexRet msgback ;
			msgback.nRet = 0 ;
			SendMsg(&msgback,sizeof(msgback)) ;
			m_bPlayerInfoDataDirty = true ;
			CLogMgr::SharedLogMgr()->SystemLog("change sex uid = %d , new sex = %d",GetPlayer()->GetUserUID(),pRet->nNewSex) ;
		}
		break;
	case MSG_PLAYER_UPDATE_MONEY:
		{
 			stMsgPlayerUpdateMoney msgUpdate ;
 			msgUpdate.nFinalCoin = GetAllCoin();
 			msgUpdate.nFinalDiamoned = GetAllDiamoned();
			msgUpdate.nCupCnt = m_stBaseData.nCupCnt ;
 			SendMsg(&msgUpdate,sizeof(msgUpdate));
		}
		break;
	//case MSG_GET_CONTINUE_LOGIN_REWARD:
	//	{
// 			stMsgGetContinueLoginReward* pGetR = (stMsgGetContinueLoginReward*)pMsg ;
// 			stMsgGetContinueLoginRewardRet msgBack ;
// 			msgBack.nRet = 0 ; //  // 0 success , 1 already getted , 2 you are not vip  ;
// 			msgBack.cRewardType = pGetR->cRewardType;
// 			msgBack.nDayIdx = m_stBaseData.nContinueDays ;
// 			msgBack.nDiamoned = GetAllDiamoned();
// 			msgBack.nFinalCoin = GetAllCoin() ;
// 			if ( m_bGivedLoginReward )
// 			{
// 				msgBack.nRet = 1 ; 
// 				SendMsgToClient((char*)&msgBack,sizeof(msgBack)) ;
// 				break;
// 			}
// 
// 			stConLoginConfig* pConfig = CGameServerApp::SharedGameServerApp()->GetConfigMgr()->GetContinueLoginConfig()->GetConfigByDayIdx(m_stBaseData.nContinueDays) ;
// 			if ( pConfig == NULL )
// 			{
// 				CLogMgr::SharedLogMgr()->ErrorLog("there is no login config for dayIdx = %d",m_stBaseData.nContinueDays ) ;
// 				msgBack.nRet = 4 ;
// 				SendMsgToClient((char*)&msgBack,sizeof(msgBack)) ;
// 				break;
// 			}
// 
// 			// give item  deponed on nContinuedDays ;
// 			if (pGetR->cRewardType == 1 )
// 			{
// 				if ( GetVipLevel() < 1 )
// 				{
// 					msgBack.nRet = 2 ;
// 					SendMsgToClient((char*)&msgBack,sizeof(msgBack)) ;
// 					break;
// 				}
// 				// give item ;
// 				for ( int i = 0 ; i < pConfig->vItems.size(); ++i )
// 				{
// 					CPlayerItemComponent* pc = (CPlayerItemComponent*)GetPlayer()->GetComponent(ePlayerComponent_PlayerItemMgr) ;
// 					pc->AddItemByID(pConfig->vItems[i].nItemID,pConfig->vItems[i].nCount) ;
// 				}
// 				// give vip prize ;
// 				m_stBaseData.nDiamoned += pConfig->nDiamoned ;
// 				msgBack.nDiamoned = GetAllDiamoned();
// 				SendMsgToClient((char*)&msgBack,sizeof(msgBack)) ;
// 				m_bGivedLoginReward = true ;
// 				break;
// 			}
// 			else if( pGetR->cRewardType == 0 )
// 			{
// 				// gvie common prize ;
// 				m_stBaseData.nCoin += pConfig->nGiveCoin ;
// 				msgBack.nFinalCoin = GetAllCoin() ;
// 				SendMsgToClient((char*)&msgBack,sizeof(msgBack)) ;
// 				m_bGivedLoginReward = true ;
// 				break;
// 			}
// 			else
// 			{
// 				msgBack.nRet = 3 ;
// 				SendMsgToClient((char*)&msgBack,sizeof(msgBack)) ;
// 				break;
// 			}
	//	}
	//	break;
	case MSG_PLAYER_REQUEST_CHARITY_STATE:
		{
 			stMsgPlayerRequestCharityStateRet msgBack ;
 			 // 0 can get charity , 1 you coin is enough , do not need charity, 2 time not reached ;
 			msgBack.nState = 0 ;

			// check times limit state ;
			time_t tNow = time(nullptr) ;
			struct tm pTimeCur, pTimeLast ;
			pTimeCur = *localtime(&tNow);
			time_t nLastTakeTime = m_stBaseData.tLastTakeCharityCoinTime;
			pTimeLast = *localtime(&nLastTakeTime);
			if ( pTimeCur.tm_year == pTimeLast.tm_year && pTimeCur.tm_mon == pTimeLast.tm_mon && pTimeCur.tm_yday == pTimeLast.tm_yday ) // the same day ; do nothing
			{

			}
			else
			{
				m_stBaseData.nTakeCharityTimes = 0 ; // new day reset times ;
			}

			msgBack.nLeftTimes = TIMES_GET_CHARITY_PER_DAY - m_stBaseData.nTakeCharityTimes ;
			if ( GetAllCoin() > COIN_CONDITION_TO_GET_CHARITY )
			{
				msgBack.nState = 1 ;
				SendMsg(&msgBack,sizeof(msgBack)) ;
				break;
			}

			if ( m_stBaseData.nTakeCharityTimes >= TIMES_GET_CHARITY_PER_DAY  )
			{
				msgBack.nState = 2 ;
				SendMsg(&msgBack,sizeof(msgBack)) ;
				break;
			}
 			SendMsg(&msgBack,sizeof(msgBack)) ;
		}
		break;
	case MSG_PLAYER_GET_CHARITY:
		{
 			stMsgPlayerGetCharityRet msgBack ;
 			// 0 success ,  1 you coin is enough , do not need charity, 2 time not reached ;
 			msgBack.nRet = 0 ;
 			msgBack.nFinalCoin = GetAllCoin();
 			msgBack.nGetCoin = 0;
 			msgBack.nLeftTimes = 0 ;
 			if ( GetAllCoin() > COIN_CONDITION_TO_GET_CHARITY )  
 			{
				msgBack.nRet = 1 ;
 				SendMsg(&msgBack,sizeof(msgBack)) ;
				break;
 			}
			
			// check times limit state ;
			time_t tNow = time(nullptr) ;
			struct tm pTimeCur ;
			struct tm pTimeLast ;
			pTimeCur = *localtime(&tNow);
			time_t nLastTakeTime = m_stBaseData.tLastTakeCharityCoinTime;
			pTimeLast = *localtime(&nLastTakeTime);
			if ( pTimeCur.tm_year == pTimeLast.tm_year && pTimeCur.tm_mon == pTimeLast.tm_mon && pTimeCur.tm_yday == pTimeLast.tm_yday ) // the same day ; do nothing
			{

			}
			else
			{
				m_stBaseData.nTakeCharityTimes = 0 ; // new day reset times ;
			}

 			if ( m_stBaseData.nTakeCharityTimes >= TIMES_GET_CHARITY_PER_DAY  )
 			{
				msgBack.nRet = 2 ;
				SendMsg(&msgBack,sizeof(msgBack)) ;
				break;
 			}
 
			++m_stBaseData.nTakeCharityTimes;
			msgBack.nGetCoin = COIN_FOR_CHARITY;
			msgBack.nLeftTimes = TIMES_GET_CHARITY_PER_DAY - m_stBaseData.nTakeCharityTimes ;
			m_stBaseData.tLastTakeCharityCoinTime = time(NULL) ;
			AddMoney(msgBack.nGetCoin);
			msgBack.nFinalCoin = GetAllCoin();
			CLogMgr::SharedLogMgr()->PrintLog("player uid = %d get charity",GetPlayer()->GetUserUID());
			m_bCommonLogicDataDirty = true ;
			m_bMoneyDataDirty = true ;

			// save log 
			stMsgSaveLog msgLog ;
			memset(msgLog.vArg,0,sizeof(msgLog.vArg));
			msgLog.nJsonExtnerLen = 0 ;
			msgLog.nLogType = eLog_GetCharity ;
			msgLog.nTargetID = GetPlayer()->GetUserUID() ;
			memset(msgLog.vArg,0,sizeof(msgLog.vArg)) ;
			msgLog.vArg[0] = GetAllCoin() ;
			SendMsg(&msgLog,sizeof(msgLog)) ;

			CLogMgr::SharedLogMgr()->SystemLog("uid = %d , final coin = %I64d",GetPlayer()->GetUserUID(),GetAllCoin());
 			SendMsg(&msgBack,sizeof(msgBack)) ;
		}
		break;
	default:
		{
			return false ;
		}
		break;
	}
	return true ;
}

bool CPlayerBaseData::onCrossServerRequest(stMsgCrossServerRequest* pRequest, eMsgPort eSenderPort,Json::Value* vJsValue )
{
	if ( IPlayerComponent::onCrossServerRequest(pRequest,eSenderPort,vJsValue) )
	{
		return true ;
	}

	switch ( pRequest->nRequestType )
	{
	case eCrossSvrReq_DeductionMoney:
		{
			assert(vJsValue&& "must not null") ;
			assert(pRequest->nTargetID == GetPlayer()->GetUserUID() && "different object");
			bool bDiamoned = !pRequest->vArg[0];
			if ( pRequest->vArg[1] < 0 || pRequest->vArg[2] < 0 )
			{
				CLogMgr::SharedLogMgr()->ErrorLog("why arg is < 0 , for cross deduction uid = %d",GetPlayer()->GetUserUID());
				return true ;
			}
			uint64_t nNeedMoney = pRequest->vArg[1] ;
			int64_t nAtLeast = pRequest->vArg[2];

			bool bRet = onPlayerRequestMoney(nNeedMoney,nAtLeast,bDiamoned) ;
			CLogMgr::SharedLogMgr()->PrintLog("uid = %d do deduction coin cross rquest , final diamond = %I64d, coin = %I64d ret = %b",GetPlayer()->GetUserUID(),m_stBaseData.nDiamoned,m_stBaseData.nCoin ,bRet );
			stMsgCrossServerRequestRet msgRet ;
			msgRet.cSysIdentifer = eSenderPort ;
			msgRet.nRet = bRet ? 0 : 1 ;
			msgRet.nRequestType = pRequest->nRequestType ;
			msgRet.nRequestSubType = pRequest->nRequestSubType;
			msgRet.nTargetID = pRequest->nReqOrigID ;
			msgRet.nReqOrigID = GetPlayer()->GetUserUID() ;
			msgRet.vArg[0] = pRequest->vArg[0];
			msgRet.vArg[1] = nNeedMoney ;

			if ( vJsValue )
			{
				Json::Value& retValue = *vJsValue ;
				CON_REQ_MSG_JSON(msgRet,retValue,autoBuf) ;
				CGameServerApp::SharedGameServerApp()->sendMsg(pRequest->nReqOrigID,autoBuf.getBufferPtr(),autoBuf.getContentSize());
			}
			else
			{
				CGameServerApp::SharedGameServerApp()->sendMsg(pRequest->nReqOrigID,(char*)&msgRet,sizeof(msgRet));
			}

			// save log 
			stMsgSaveLog msgLog ;
			memset(msgLog.vArg,0,sizeof(msgLog.vArg));
			msgLog.nJsonExtnerLen = 0 ;
			msgLog.nLogType = eLog_DeductionMoney ;
			msgLog.nTargetID = GetPlayer()->GetUserUID() ;
			msgLog.vArg[0] = !bDiamoned ;
			msgLog.vArg[1] = bRet ? nNeedMoney : 0;
			msgLog.vArg[2] = m_stBaseData.nCoin;
			msgLog.vArg[3] = m_stBaseData.nDiamoned ;
			msgLog.vArg[4] = pRequest->nRequestSubType ;
			if ( eCrossSvrReqSub_TaxasSitDown == pRequest->nRequestSubType )
			{
				msgLog.vArg[5] = pRequest->nReqOrigID ;
			}
			CGameServerApp::SharedGameServerApp()->sendMsg(pRequest->nReqOrigID,(char*)&msgLog,sizeof(msgLog));

		}
		break;
	case eCrossSvrReq_AddMoney:
		{
			bool bDiamoned = !pRequest->vArg[0];
			int64_t nAddCoin = pRequest->vArg[1] ;
			if ( nAddCoin < 0 )
			{
				CLogMgr::SharedLogMgr()->ErrorLog("why add coin is < 0  uid = %d",GetPlayer()->GetUserUID());
				return true ;
			}

			uint32_t& nAddTarget = bDiamoned ? m_stBaseData.nDiamoned : m_stBaseData.nCoin ; 
			nAddTarget += nAddCoin ;
			m_bMoneyDataDirty = true ;
			CLogMgr::SharedLogMgr()->PrintLog("uid = %d do add coin cross rquest , final diamond = %I64d, coin = %I64d",GetPlayer()->GetUserUID(),m_stBaseData.nDiamoned,m_stBaseData.nCoin );

			// save log 
			stMsgSaveLog msgLog ;
			memset(msgLog.vArg,0,sizeof(msgLog.vArg));
			msgLog.nJsonExtnerLen = 0 ;
			msgLog.nLogType = eLog_AddMoney ;
			msgLog.nTargetID = GetPlayer()->GetUserUID() ;
			msgLog.vArg[0] = !bDiamoned ;
			msgLog.vArg[1] = nAddCoin;
			msgLog.vArg[2] = m_stBaseData.nCoin;
			msgLog.vArg[3] = m_stBaseData.nDiamoned ;
			msgLog.vArg[4] = pRequest->nRequestSubType ;
			if ( eCrossSvrReqSub_TaxasSitDownFailed == pRequest->nRequestSubType || eCrossSvrReqSub_TaxasStandUp == pRequest->nRequestSubType )
			{
				msgLog.vArg[5] = pRequest->nReqOrigID ;
			}
			CGameServerApp::SharedGameServerApp()->sendMsg(pRequest->nReqOrigID,(char*)&msgLog,sizeof(msgLog));
		}
		break;
	case eCrossSvrReq_SyncCoin:
		{
			m_stBaseData.nCoin = pRequest->vArg[0] ;
			if ( m_stBaseData.nCoin < 0 )
			{
				m_stBaseData.nCoin = 0 ;
				m_bMoneyDataDirty = true ;
			}

			if ( pRequest->vArg[0] )
			{
				m_bMoneyDataDirty = true ;
			}
		}
		break;
	default:
		return false;
	}
	return true ;
}

bool CPlayerBaseData::onCrossServerRequestRet(stMsgCrossServerRequestRet* pResult,Json::Value* vJsValue )
{
	if ( IPlayerComponent::onCrossServerRequestRet(pResult,vJsValue) )
	{
		return true ;
	}
	return false ;
}

void CPlayerBaseData::SendBaseDatToClient()
{
	if ( nReadingDataFromDB == 2 )
	{
		Json::Value jValue ;
		jValue["name"] = m_stBaseData.cName ;
		jValue["coin"] = m_stBaseData.nCoin ;
		jValue["sex"] = m_stBaseData.nSex ;
		jValue["diamond"] = m_stBaseData.nDiamoned ;
		SendMsg(jValue,MSG_PLAYER_BASE_DATA);
		CLogMgr::SharedLogMgr()->PrintLog("send base data to session id = %d ",GetPlayer()->GetSessionID() );
		CLogMgr::SharedLogMgr()->SystemLog("send data uid = %d , final coin = %d, sex = %d",GetPlayer()->GetUserUID(),GetAllCoin(),m_stBaseData.nSex);
	}
	else
	{
		CLogMgr::SharedLogMgr()->ErrorLog("uid = %u not read from db , why send to client ?") ;
	}
}

void CPlayerBaseData::OnProcessContinueLogin( bool bNewDay, time_t nLastLogin)
{
 	if ( nLastLogin == 0 )
 	{
 		m_stBaseData.nContinueDays = 1 ;
 	}
 	else
 	{
 		time_t nCur = time(NULL) ;
 		struct tm* pTempTimer = NULL;
 		pTempTimer = localtime(&nCur) ;
 		struct tm pTimeCur ;
 		if ( pTempTimer )
 		{
 			pTimeCur = *pTempTimer ;
 		}
 		else
 		{
 			CLogMgr::SharedLogMgr()->ErrorLog("local time return null ?") ;
 		}
		
		time_t tLastLoginTime = m_stBaseData.tLastLoginTime;
 		pTempTimer = localtime(&tLastLoginTime) ;
 		struct tm pTimeLastLogin  ;
 		if ( pTempTimer )
 		{
 			pTimeLastLogin = *pTempTimer ;
 		}
 		else
 		{
 			CLogMgr::SharedLogMgr()->ErrorLog("local time return null ?") ;
 		}
 		
 		if ( pTimeCur.tm_year == pTimeLastLogin.tm_year && pTimeCur.tm_mon == pTimeLastLogin.tm_mon && pTimeCur.tm_yday == pTimeLastLogin.tm_yday )
 		{
 			m_stBaseData.tLastLoginTime = (unsigned int)nCur ;
 			m_bGivedLoginReward = true ;
 			return ; // do nothing ; same day ;
 		}
 
 		double nDiffe = difftime(nCur,m_stBaseData.tLastLoginTime) ;
 		bool bContine = abs(nDiffe) - 60 * 60 * 24 <= 0 ;
 		
 		if ( bContine )
 		{
 			++m_stBaseData.nContinueDays ;   // real contiune ;
 		}
 		else
 		{
 			m_stBaseData.nContinueDays = 1 ;    // disturbed ;
 		}
 
 		m_stBaseData.tLastLoginTime = (unsigned int)nCur ;
 	}
}

void CPlayerBaseData::onRecivedLoginData()
{
	time_t nCur = time(NULL) ;
	time_t nLastLogin = m_stBaseData.tLastLoginTime;
	struct tm pTimeCur = *localtime(&nCur);
	struct tm pTimeLastLogin = *localtime(&nLastLogin);

	bool bSameDay = (pTimeCur.tm_year == pTimeLastLogin.tm_year && pTimeCur.tm_mon == pTimeLastLogin.tm_mon && pTimeCur.tm_yday == pTimeLastLogin.tm_yday ) ;
	// process yesterday win ;
	OnProcessContinueLogin(bSameDay,nLastLogin);

	// process yesterday win coin 
	if ( bSameDay == false )
	{
		m_stBaseData.nTodayGameCoinOffset = 0 ;
	}
	m_stBaseData.tLastLoginTime = nCur ;
	m_bCommonLogicDataDirty = true ;
}

void CPlayerBaseData::TimerSave()
{
	if ( nReadingDataFromDB != 2 )
	{
		CLogMgr::SharedLogMgr()->ErrorLog("uid = %u , not finish read why save to db ? ",GetPlayer()->GetUserUID()) ;
		m_bMoneyDataDirty = false ;
		m_bCommonLogicDataDirty = false;
		m_bPlayerInfoDataDirty = false;
		return ;
	}

	if ( m_bMoneyDataDirty )
	{
		m_bMoneyDataDirty = false ;
		stMsgSavePlayerMoney msgSaveMoney ;
		msgSaveMoney.nCoin = m_stBaseData.nCoin + m_nTempCoin;
		msgSaveMoney.nDiamoned = m_stBaseData.nDiamoned;
		msgSaveMoney.nUserUID = GetPlayer()->GetUserUID() ;
		msgSaveMoney.nCupCnt = m_stBaseData.nCupCnt ;
		SendMsg((stMsgSavePlayerMoney*)&msgSaveMoney,sizeof(msgSaveMoney)) ;
		CLogMgr::SharedLogMgr()->SystemLog("player do time save coin uid = %d coin = %I64d",msgSaveMoney.nUserUID,msgSaveMoney.nCoin + m_nTempCoin );
	}

	if ( m_bCommonLogicDataDirty )
	{
		m_bCommonLogicDataDirty = false ;
		stMsgSavePlayerCommonLoginData msgLogicData ;
		msgLogicData.dfLatidue = m_stBaseData.dfLatidue ;
		msgLogicData.dfLongitude = m_stBaseData.dfLongitude ;
		msgLogicData.nContinueDays = m_stBaseData.nContinueDays ;
		msgLogicData.nNewPlayerHaloWeight = m_stBaseData.nNewPlayerHaloWeight ;
		//msgLogicData.nExp = m_stBaseData.nExp ;
		msgLogicData.nMostCoinEver = m_stBaseData.nMostCoinEver;
		msgLogicData.nTodayCoinOffset = m_stBaseData.nTodayGameCoinOffset ;
		msgLogicData.nTotalGameCoinOffset = m_stBaseData.nTotalGameCoinOffset ;
		msgLogicData.nYesterdayCoinOffset = m_stBaseData.nYesterdayCoinOffset ;
		msgLogicData.nUserUID = GetPlayer()->GetUserUID() ;
		msgLogicData.nCardType = m_stBaseData.nCardType ;
		msgLogicData.nCardEndTime = m_stBaseData.nCardEndTime ;
		msgLogicData.nLastTakeCardGiftTime = m_stBaseData.tLastTakeCardGiftTime ;
		msgLogicData.nTakeCharityTimes = m_stBaseData.nTakeCharityTimes ;
		msgLogicData.nTotalInvitePrizeCoin = m_stBaseData.nTotalInvitePrizeCoin ;


		msgLogicData.nVipLevel = m_stBaseData.nVipLevel ;
		msgLogicData.tLastLoginTime = m_stBaseData.tLastLoginTime ;
		msgLogicData.tLastTakeCharityCoinTime = m_stBaseData.tLastTakeCharityCoinTime ;
		msgLogicData.tOfflineTime = m_stBaseData.tOfflineTime ;
		memcpy(msgLogicData.vJoinedClubID,m_stBaseData.vJoinedClubID,sizeof(msgLogicData.vJoinedClubID));
		SendMsg((stMsgSavePlayerMoney*)&msgLogicData,sizeof(msgLogicData)) ;
	}

	if ( m_bPlayerInfoDataDirty )
	{
		m_bPlayerInfoDataDirty = false ;
		stMsgSavePlayerInfo msgSaveInfo ;
		msgSaveInfo.nPhotoID = m_stBaseData.nPhotoID ;
		msgSaveInfo.nIsRegister = m_stBaseData.isRegister ;
		msgSaveInfo.nSex = m_stBaseData.nSex ;
		msgSaveInfo.nUserUID = GetPlayer()->GetUserUID() ;
		msgSaveInfo.nInviterUID = m_stBaseData.nInviteUID ;
		memcpy(msgSaveInfo.vName,m_stBaseData.cName,sizeof(msgSaveInfo.vName));
		memcpy(msgSaveInfo.vSigure,m_stBaseData.cSignature,sizeof(msgSaveInfo.vSigure));
		memcpy(msgSaveInfo.vUploadedPic,m_stBaseData.vUploadedPic,sizeof(msgSaveInfo.vUploadedPic));
		SendMsg((stMsgSavePlayerMoney*)&msgSaveInfo,sizeof(msgSaveInfo)) ;
	}
}

bool CPlayerBaseData::onPlayerRequestMoney(uint64_t& nCoinOffset,uint64_t nAtLeast, bool bDiamoned)
{
	bool invalidAtLeast = (nAtLeast != 0 && nAtLeast < nCoinOffset );

 	if ( bDiamoned == false )
 	{
 		if ( nCoinOffset > GetAllCoin() )
		{
			if ( invalidAtLeast && GetAllCoin() >= nAtLeast )
			{
				nCoinOffset = nAtLeast ;
				m_stBaseData.nCoin -= nCoinOffset ;
				m_bMoneyDataDirty = true ;
				return true ;
			}
			return false ;
		}
 		//m_nTaxasPlayerCoin += nCoinOffset ;   //add after recieved comfirm msg 
 		m_stBaseData.nCoin -= nCoinOffset ;
 	}
 	else
 	{
		if ( nCoinOffset > GetAllDiamoned() )
		{
			if ( invalidAtLeast && GetAllDiamoned() >= nAtLeast )
			{
				nCoinOffset = nAtLeast ;
				m_stBaseData.nDiamoned -= nCoinOffset ;
				m_bMoneyDataDirty = true ;
				return true ;
			}

			return false ;
		}
 		// m_nTaxasPlayerDiamoned += nCoinOffset; ; //add after recieved comfirm msg 
 		m_stBaseData.nDiamoned -= nCoinOffset ;
 	}
	m_bMoneyDataDirty = true ;
	return true ;
}

bool CPlayerBaseData::AddMoney(int32_t nOffset,bool bDiamond  )
{
	if ( bDiamond )
	{
		m_stBaseData.nDiamoned += (int)nOffset ;
	}
	else
	{
		auto pcom = (CPlayerGameData*)GetPlayer()->GetComponent(ePlayerComponent_PlayerGameData) ;
		if ( pcom->isNotInAnyRoom() )
		{
			m_stBaseData.nCoin += nOffset ;
		}
		else
		{
			m_nTempCoin += nOffset ;
		}

		if ( m_stBaseData.nCoin > m_stBaseData.nMostCoinEver )
		{
			m_stBaseData.nMostCoinEver = m_stBaseData.nCoin ;
			m_bCommonLogicDataDirty = true ;
		}
	}
	m_bMoneyDataDirty = true ;
	return true ;
}

void CPlayerBaseData::addInvitePrize(uint32_t nCoinPrize )
{
	AddMoney(nCoinPrize);
	m_stBaseData.nTotalInvitePrizeCoin += nCoinPrize ;
	m_bCommonLogicDataDirty = true ;
}

bool CPlayerBaseData::decressMoney(int64_t nOffset,bool bDiamond )
{
	if ( bDiamond )
	{
		if ( m_stBaseData.nDiamoned < nOffset )
		{
			return false ;
		}
		m_stBaseData.nDiamoned -= nOffset ;
		m_bMoneyDataDirty = true ;
		return true ;
	}

	if ( m_stBaseData.nCoin < nOffset )
	{
		return false ;
	}
	m_stBaseData.nCoin -= nOffset ;
	m_bMoneyDataDirty = true ;
	return true ;
}

bool CPlayerBaseData::OnPlayerEvent(stPlayerEvetArg* pArg)
{
	return false ;
}

void CPlayerBaseData::GetPlayerBrifData(stPlayerBrifData* pData )
{
	if ( !pData )
	{
		return ;
	}
	memcpy(pData,&m_stBaseData,sizeof(stPlayerBrifData));
	auto pGameData = (CPlayerGameData*)GetPlayer()->GetComponent(ePlayerComponent_PlayerGameData);
	pData->nCurrentRoomID = pGameData->getCurRoomID() * 10 + pGameData->getCurRoomType();
}

void CPlayerBaseData::GetPlayerDetailData(stPlayerDetailData* pData )
{
	if ( !pData )
	{
		return ;
	}
	memcpy(pData,&m_stBaseData,sizeof(stPlayerDetailData));
}

bool CPlayerBaseData::EventFunc(void* pUserData,stEventArg* pArg)
{
	auto pp = (CPlayerBaseData*)pUserData ;
	pp->OnNewDay(pArg);
	return false ;
}

void CPlayerBaseData::OnNewDay(stEventArg* pArg)
{
	m_stBaseData.nTodayGameCoinOffset = 0 ;
	m_bCommonLogicDataDirty = true ;
}

void CPlayerBaseData::OnReactive(uint32_t nSessionID )
{
	CEventCenter::SharedEventCenter()->RegisterEventListenner(eEvent_NewDay,this,CPlayerBaseData::EventFunc ) ;
	CLogMgr::SharedLogMgr()->PrintLog("player reactive send base data");

	if ( nReadingDataFromDB != 2 )
	{
		stMsgDataServerGetBaseData msg ;
		msg.nUserUID = GetPlayer()->GetUserUID() ;
		SendMsg(&msg,sizeof(msg)) ;
		nReadingDataFromDB = 1 ;
		CLogMgr::SharedLogMgr()->ErrorLog("still not ready player data , wait a moment uid = %u",GetPlayer()->GetUserUID()) ;
	}
	else
	{
		SendBaseDatToClient();
		onRecivedLoginData();
	}

	stMsgRequestClientIp msgReq ;
	SendMsg(&msgReq,sizeof(msgReq)) ; 
	CLogMgr::SharedLogMgr()->PrintLog("send request ip , sessioni id = %d",GetPlayer()->GetSessionID()) ;
}

void CPlayerBaseData::OnOtherDoLogined()
{ 	
	stMsgRequestClientIp msgReq ;
	SendMsg(&msgReq,sizeof(msgReq)) ; 
	CLogMgr::SharedLogMgr()->PrintLog("send request ip , sessioni id = %d",GetPlayer()->GetSessionID()) ;

	if ( nReadingDataFromDB != 2 )
	{
		stMsgDataServerGetBaseData msg ;
		msg.nUserUID = GetPlayer()->GetUserUID() ;
		SendMsg(&msg,sizeof(msg)) ;
		nReadingDataFromDB = 1 ;
		CLogMgr::SharedLogMgr()->ErrorLog("still not ready player data , wait a moment uid = %u",GetPlayer()->GetUserUID()) ;
	}
	else
	{
		SendBaseDatToClient();
		onRecivedLoginData();
	}
}

void CPlayerBaseData::OnPlayerDisconnect()
{
	IPlayerComponent::OnPlayerDisconnect();

	TimerSave();
	CEventCenter::SharedEventCenter()->RemoveEventListenner(eEvent_NewDay,this,CPlayerBaseData::EventFunc ) ;
}

void CPlayerBaseData::OnOtherWillLogined()
{
	IPlayerComponent::OnOtherWillLogined();
}

bool CPlayerBaseData::isPlayerRegistered()
{
	return m_stBaseData.isRegister ;
}

uint8_t CPlayerBaseData::getNewPlayerHaloWeight()
{
	return m_stBaseData.nNewPlayerHaloWeight ;
}

void CPlayerBaseData::setNewPlayerHalo(uint8_t nPlayHalo )
{
	if ( nPlayHalo == m_stBaseData.nNewPlayerHaloWeight )
	{
		return ;
	}

	if ( nPlayHalo > MAX_NEW_PLAYER_HALO )
	{
		m_stBaseData.nNewPlayerHaloWeight = 0;
		CLogMgr::SharedLogMgr()->ErrorLog("uid = %u, set halo big than 100  = %u",GetPlayer()->GetUserUID(),nPlayHalo);
	}
	else
	{
		m_stBaseData.nNewPlayerHaloWeight = nPlayHalo;
		CLogMgr::SharedLogMgr()->PrintLog("uid = %u, set halo  = %u",GetPlayer()->GetUserUID(),nPlayHalo);
	}

	m_bCommonLogicDataDirty = true ;
}

void CPlayerBaseData::onGetReward( uint8_t nIdx ,uint16_t nRewardID, uint16_t nGameType ,const char* nRoomName  )
{
	auto Reward = CRewardConfig::getInstance()->getRewardByID(nRewardID) ;
	if ( Reward == nullptr )
	{
		CLogMgr::SharedLogMgr()->ErrorLog("uid = %d get reward is null reward id = %d",GetPlayer()->GetUserUID(),nRewardID) ;
		return  ;
	}
	CLogMgr::SharedLogMgr()->PrintLog("uid = %d get reward id = %d",GetPlayer()->GetUserUID(),nRewardID) ;

	if ( Reward->nCupCnt )
	{
		m_stBaseData.nCupCnt += Reward->nCupCnt ;
		m_bMoneyDataDirty = true ;
	}

	if ( Reward->nCoin )
	{
		 AddMoney(Reward->nCoin) ;
	}

	if ( Reward->nDiamond )
	{
		AddMoney(Reward->nDiamond,true) ;
	}

	Json::Value jValue ;
	jValue["gameType"] = nGameType ;
	jValue["roomName"] = nRoomName ;
	jValue["rankIdx"] = nIdx ;
	jValue["addCoin"] = Reward->nCoin ;
	jValue["cup"] = Reward->nCupCnt ;
	jValue["diamomd"] = Reward->nDiamond ;
	Json::StyledWriter writers ;
	std::string strContent = writers.write(jValue);
	CPlayerMailComponent::PostMailToPlayer(eMail_WinMatch,strContent.c_str(),strContent.size(),GetPlayer()->GetUserUID()) ;
}

void CPlayerBaseData::setCoin(int64_t nCoin )
{ 
	if (m_stBaseData.nCoin != nCoin)
	{
		m_bMoneyDataDirty =  true;
	}
	m_stBaseData.nCoin = nCoin ; 
}

void CPlayerBaseData::setTempCoin( uint32_t nTempCoin )
{ 
	if ( m_nTempCoin != nTempCoin )
	{
		m_bMoneyDataDirty = true ;
	}
	m_nTempCoin = nTempCoin ;
}

void CPlayerBaseData::addTodayGameCoinOffset(int32_t nOffset )
{
	if ( nOffset == (int32_t)0 )
	{
		return ;
	}
	m_stBaseData.nTotalGameCoinOffset += nOffset ;
	m_stBaseData.nTodayGameCoinOffset += nOffset ; 
	this->m_bCommonLogicDataDirty = true ; 
	CLogMgr::SharedLogMgr()->PrintLog("update game coin offset uid = %u , today offset = %I64d, total = %d , offset = %d",GetPlayer()->GetUserUID(),m_stBaseData.nTodayGameCoinOffset,m_stBaseData.nTotalGameCoinOffset,nOffset);
}

