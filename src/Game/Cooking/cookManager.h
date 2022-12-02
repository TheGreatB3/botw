#pragma once

#include <container/seadSafeArray.h>
#include <container/seadTreeMap.h>
#include <heap/seadDisposer.h>
#include "KingSystem/Resource/resHandle.h"
#include "KingSystem/Utils/Byaml/Byaml.h"
#include "KingSystem/Utils/Types.h"

namespace uking {

struct CookArg;
struct CookIngredient;

// TODO: Find actual type
struct UnkItem;

enum class CookEffectId : s32 {
    None = -1,
    LifeRecover = 1,
    LifeMaxUp = 2,
    ResistHot = 4,
    ResistCold = 5,
    ResistElectric = 6,
    AttackUp = 10,
    DefenseUp = 11,
    Quietness = 12,
    MovingSpeed = 13,
    GutsRecover = 14,
    ExGutsMaxUp = 15,
    Fireproof = 16,
};

struct CookItem {
    CookItem();

    void reset();
    void copy(CookItem& to) const;

    sead::FixedSafeString<64> actor_name{""};
    sead::SafeArray<sead::FixedSafeString<64>, 5> ingredients;
    f32 life_recover{};
    s32 effect_time{};
    s32 item_price{};
    CookEffectId effect_id = CookEffectId::None;
    f32 stamina_recover{};
    bool _224{};
};
KSYS_CHECK_SIZE_NX150(CookItem, 0x228);

// TODO
class CookingMgr {
    SEAD_SINGLETON_DISPOSER(CookingMgr)
public:
    static constexpr s32 NumIngredientsMax = 5;
    static constexpr s32 NumEffects = 13;
    static constexpr s32 NumEffectSlots = 17;

    struct Ingredient {
        u32 name_hash;
        int _4;
        const CookIngredient* arg;
        bool _10;
        al::ByamlIter actor_data;
    };

    struct BoostArg {
        bool always_boost;
        bool enable_random_boost;
    };

    CookingMgr();
    ~CookingMgr();

    void cookFail(CookItem& item);
    void cookFailForMissingConfig(CookItem& item, const sead::SafeString& name);
    void cookCalcBoost(const Ingredient* ingredients, CookItem& item,
                       const BoostArg* boost_arg) const;
    void cookHandleBoostMonsterExtractInner([[maybe_unused]] const Ingredient* ingredients,
                                            CookItem& item) const;
    void cookHandleBoostSuccessInner([[maybe_unused]] const Ingredient ingredients[],
                                     CookItem& item) const;
    void cookCalc3(const Ingredient ingredients[], CookItem& item);
    void cookCalcItemPrice(const Ingredient ingredients[], CookItem& item) const;
    void cookCalc1(const Ingredient ingredients[], CookItem& item);

    void init(sead::Heap* heap);

    bool cook(const CookArg& arg, const CookItem& cook_item, const BoostArg& boost_arg);

    bool resetArgCookData(const CookArg& arg, const CookItem& item);

    bool cookWithItems(const CookArg& arg, const UnkItem& item1, const UnkItem& item2,
                       const UnkItem& item3, const UnkItem& item4, const UnkItem& item5,
                       const CookItem& cook_item, const BoostArg& boost_arg);

    void setCookItem(const CookItem& from);
    void resetCookItem();
    void getCookItem(CookItem& to) const;

private:
    struct CookingEffectEntry {
        int bt = 0;
        int ma = 0;
        int mi = 0;
        float mr = 1.0f;
        int ssa = 1;
    };

    al::ByamlIter* mConfig = nullptr;

    ksys::res::Handle mResHandle;

    sead::FixedSafeString<64> mFailActorName;
    sead::FixedSafeString<64> mFairyTonicName;
    sead::FixedSafeString<64> mMonsterExtractName;

    u32 mFailActorNameHash = 0;
    u32 mFairyTonicNameHash = 0;
    u32 mMonsterExtractNameHash = 0;

    float mLRMR = 1.0f;
    float mFALRMR = 1.0f;
    u32 mFALR = 4;
    u32 mSFALR = 1;
    u32 mSSAET = 300;

    sead::SafeArray<CookingEffectEntry, NumEffectSlots> mCookingEffectEntries;

    sead::SafeArray<float, NumIngredientsMax> mNMMR;

    sead::SafeArray<int, NumIngredientsMax> mNMSSR;

    CookItem mCookItem;

    sead::FixedTreeMap<u32, CookEffectId, NumEffects> mCookingEffectNameIdMap{};
};
KSYS_CHECK_SIZE_NX150(CookingMgr, 0x7D8);

struct CookIngredient {
    sead::FixedSafeString<64> name;
    int _58;
};
KSYS_CHECK_SIZE_NX150(CookIngredient, 0x60);

struct CookArg {
    CookIngredient ingredients[CookingMgr::NumIngredientsMax];
};
KSYS_CHECK_SIZE_NX150(CookArg, 0x1E0);

}  // namespace uking