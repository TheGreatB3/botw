#pragma once

#include <container/seadObjArray.h>
#include <heap/seadDisposer.h>
#include "KingSystem/Resource/resHandle.h"
#include "KingSystem/Utils/Byaml/Byaml.h"
#include "KingSystem/Utils/Types.h"
#include "cookItem.h"

namespace ksys {

struct CookArg;
struct CookIngredient;

// TODO: Find actual type
struct UnkItem;

// TODO
class CookingMgr {
    SEAD_SINGLETON_DISPOSER(CookingMgr)
public:
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
    void cookFailForMissingConfig(CookItem& item, const sead::FixedSafeString<64>& name);
    void cookCalcBoost(const Ingredient ingredients[], CookItem& item, const BoostArg& boost_arg);
    void cookHandleBoostSuccessInner(const Ingredient ingredients[], CookItem& item);
    void cookCalc3(const Ingredient ingredients[], CookItem& item);
    void cookCalcItemPrice(const Ingredient ingredients[], CookItem& item);
    void cookCalc1(const Ingredient ingredients[], CookItem& item);

    void init(sead::Heap* heap);

    bool cook(const CookArg& arg, const CookItem& cookItem, const BoostArg& boostArg);

    bool resetArgCookData(const CookArg& arg, const CookItem& item);

    bool cookWithItems(const CookArg& arg, const UnkItem& item1, const UnkItem& item2,
                       const UnkItem& item3, const UnkItem& item4, const UnkItem& item5,
                       const CookItem& cook_item, const BoostArg& boost_arg);

    void getCookItem(CookItem& to);

    void x(CookItem& to);

private:
    struct CookingEffectEntry {
        int bt = 0;
        int ma = 0;
        int mi = 0;
        float mr = 1.0f;
        int ssa = 1;
    };

    al::ByamlIter* mConfig = nullptr;

    ksys::res::Handle mRes2;

    sead::FixedSafeString<64> mFailActor;
    sead::FixedSafeString<64> mFairyTonicName;
    sead::FixedSafeString<64> mMonsterExtractName;

    int mFailActorNameHash = 0;
    int mFairyTonicNameHash = 0;
    u32 mMonsterExtractNameHash = 0;

    float mLRMR = 1.0f;
    float mFALRMR = 1.0f;
    u32 mFALR = 4;
    u32 mSFALR = 1;
    u32 mSSAET = 300;

    CookingEffectEntry mCookingEffectEntries[16];

    u32 _2E0 = 0;

    __attribute__((packed)) __attribute__((aligned(1))) u64 _2E4 = 0;

    int _2EC = 1;

    float _2F0 = 1.0f;

    float mNMMR[5];

    int mNMSSR[5];

    int _31C;

    CookItem mCookItem;

    sead::FixedObjArray<s64[5], 13> _548{};
};
KSYS_CHECK_SIZE_NX150(CookingMgr, 0x7D8);

struct CookIngredient {
    sead::FixedSafeString<64> name;
    int _58;
};
KSYS_CHECK_SIZE_NX150(CookIngredient, 0x60);

struct CookArg {
    CookIngredient ingredients[5];
};
KSYS_CHECK_SIZE_NX150(CookArg, 0x1E0);

}  // namespace ksys
