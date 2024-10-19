//Reward system made by Talamortis

#include "Configuration/Config.h"
#include "Player.h"
#include "AccountMgr.h"
#include "ScriptMgr.h"
#include "Define.h"
#include "GossipDef.h"
#include "Chat.h"

bool RewardSystem_Enable;
uint32 Max_roll;

class reward_system : public PlayerScript
{

public:
    reward_system() : PlayerScript("reward_system") {}

    uint32 initialTimer = (sConfigMgr->GetOption<uint32>("RewardTime", 1) * HOUR * IN_MILLISECONDS);
    uint32 RewardTimer = initialTimer;
    int32 roll;

    void OnLogin(Player* player)  override
    {
        if (sConfigMgr->GetOption<bool>("RewardSystemEnable", true) && sConfigMgr->GetOption<bool>("RewardSystem.Announce", true)) {
            ChatHandler(player->GetSession()).SendSysMessage("本服务器正在运行 |cff4CFF00时间奖励 |r模块.");
        }
    }

    void OnBeforeUpdate(Player* player, uint32 p_time) override
    {
        if (sConfigMgr->GetOption<bool>("RewardSystemEnable", true))
        {
            if (RewardTimer > 0)
            {
                if (player->isAFK())
                    return;

                if (RewardTimer <= p_time)
                {
                    roll = urand(1, Max_roll);
                    // TODO: this should use a asynchronous query with a callback instead of a synchronous, blocking query
                    QueryResult result = CharacterDatabase.Query("SELECT item, quantity FROM reward_system WHERE roll = '{}'", roll);

                    if (!result)
                    {
                        ChatHandler(player->GetSession()).PSendSysMessage("[奖励系统] 祝你下次好运！你的掷骰结果是 {}.", roll);
                        RewardTimer = initialTimer;
                        return;
                    }

                    //Lets now get the item
                    do
                    {
                        Field* fields = result->Fetch();
                        uint32 pItem = fields[0].Get<int32>();
                        uint32 quantity = fields[1].Get<int32>();

                        // now lets add the item
                        //player->AddItem(pItem, quantity);
                        SendRewardToPlayer(player, pItem, quantity);
                    } while (result->NextRow());

                    ChatHandler(player->GetSession()).PSendSysMessage("[奖励系统] 恭喜你，你以掷骰子的结果{}获胜了.", roll);

                    RewardTimer = initialTimer;
                }
                else  RewardTimer -= p_time;
            }
        }
    }

    void SendRewardToPlayer(Player* receiver, uint32 itemId, uint32 count)
    {
        if (receiver->IsInWorld() && receiver->AddItem(itemId, count))
            return;

        ChatHandler(receiver->GetSession()).PSendSysMessage("你会在邮箱中收到获得的奖品");
        // format: name "subject text" "mail text" item1[:count1] item2[:count2] ... item12[:count12]
        uint64 receiverGuid = receiver->GetGUID().GetCounter();
        std::string receiverName;

        std::string subject = "奖励系统奖品";
        std::string text = "祝贺你，你赢得了一个奖品!";

        ItemTemplate const* item_proto = sObjectMgr->GetItemTemplate(itemId);

        if (!item_proto)
        {
            LOG_ERROR("module", "[奖励系统] 物品ID无效: {}", itemId);
            return;
        }

        if (count < 1 || (item_proto->MaxCount > 0 && count > uint32(item_proto->MaxCount)))
        {
            LOG_ERROR("module", "[奖励系统] 物品数量无效: {} : {}", itemId, count);
            return;
        }

        typedef std::pair<uint32, uint32> ItemPair;
        typedef std::list< ItemPair > ItemPairs;
        ItemPairs items;

        while (count > item_proto->GetMaxStackSize())
        {
            items.push_back(ItemPair(itemId, item_proto->GetMaxStackSize()));
            count -= item_proto->GetMaxStackSize();
        }

        items.push_back(ItemPair(itemId, count));

        if (items.size() > MAX_MAIL_ITEMS)
        {
            LOG_ERROR("module", "[奖励系统] 最大邮寄物品数为 {}, 当前大小: {}", MAX_MAIL_ITEMS, items.size());
            return;
        }

        // from console show not existed sender
        MailSender sender(MAIL_NORMAL, receiver->GetSession() ? receiver->GetGUID().GetCounter() : 0, MAIL_STATIONERY_TEST);

        // fill mail
        MailDraft draft(subject, text);

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        for (ItemPairs::const_iterator itr = items.begin(); itr != items.end(); ++itr)
        {
            if (Item* item = Item::CreateItem(itr->first, itr->second, receiver->GetSession() ? receiver : 0))
            {
                item->SaveToDB(trans);                               // save for prevent lost at next mail load, if send fail then item will deleted
                draft.AddItem(item);
            }
        }

        draft.SendMailTo(trans, MailReceiver(receiver, receiverGuid), sender);
        CharacterDatabase.CommitTransaction(trans);

        return;
    }

};

class reward_system_conf : public WorldScript
{
public:
    reward_system_conf() : WorldScript("reward_system_conf") { }

    void OnBeforeConfigLoad(bool reload) override
    {
        if (!reload) {
            Max_roll = sConfigMgr->GetOption<uint32>("MaxRoll", 1000);
        }
    }
};

void AddRewardSystemScripts()
{
    new reward_system();
    new reward_system_conf();
}
