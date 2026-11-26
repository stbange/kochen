#ifndef KOCHEN_TYPES_H
#define KOCHEN_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KOCHEN_MAX_INGREDIENTS   192
#define KOCHEN_MAX_ARCHETYPES    20
#define KOCHEN_MAX_PANTRIES      16
#define KOCHEN_MAX_ARCH_SLOTS    10
#define KOCHEN_MAX_PATCH_SLOTS   20
#define KOCHEN_MAX_DAG_NODES     28
#define KOCHEN_SLUG_LEN          36
#define KOCHEN_NAME_LEN          80
#define KOCHEN_STEP_TEXT_LEN     384
#define KOCHEN_MAX_LOCALE_KEYS   512

typedef enum {
    KOCHEN_LANG_EN = 0,
    KOCHEN_LANG_DE = 1,
    KOCHEN_LANG_PL = 2,
    KOCHEN_LANG_COUNT = 3
} kochen_lang_t;

typedef enum {
    KOCHEN_MODE_HOME = 0,
    KOCHEN_MODE_PRO  = 1
} kochen_mode_t;

typedef enum {
    KOCHEN_PROT_NONE = 0,
    KOCHEN_PROT_COLLAGEN = 1,
    KOCHEN_PROT_MYOFIBRILLAR = 2,
    KOCHEN_PROT_OVALBUMIN = 3,
    KOCHEN_PROT_CASEIN = 4,
    KOCHEN_PROT_WHEY = 5,
    KOCHEN_PROT_GLUTEN = 6,
    KOCHEN_PROT_PLANT_GLOBULIN = 7
} kochen_protein_class_t;

typedef enum {
    KOCHEN_ROLE_STARCH_MATRIX       = 0,
    KOCHEN_ROLE_PROTEIN_COLLAGEN    = 1,
    KOCHEN_ROLE_PROTEIN_TENDER      = 2,
    KOCHEN_ROLE_LEGUME_MATRIX       = 3,
    KOCHEN_ROLE_AQUEOUS_BASE        = 4,
    KOCHEN_ROLE_FAT_MEDIUM          = 5,
    KOCHEN_ROLE_SURFACTANT_BINDER   = 6,
    KOCHEN_ROLE_HYDROCOLLOID_GEL    = 7,
    KOCHEN_ROLE_UMAMI_FERMENT       = 8,
    KOCHEN_ROLE_AROMA_EARLY_FAT     = 9,
    KOCHEN_ROLE_AROMA_LATE_VOLATILE = 10,
    KOCHEN_ROLE_ACID_BRIGHTENER     = 11,
    KOCHEN_ROLE_SWEET_CARAMEL       = 12,
    KOCHEN_ROLE_TEXTURE_CRISP       = 13,
    KOCHEN_ROLE_COUNT               = 14
} kochen_role_t;

/* Volatile compound family flags (16-bit) */
#define KOCHEN_VOL_PYRAZINES        (1u << 0)
#define KOCHEN_VOL_LACTONES         (1u << 1)
#define KOCHEN_VOL_TERPENES_CITRUS  (1u << 2)
#define KOCHEN_VOL_TERPENES_HERBAL  (1u << 3)
#define KOCHEN_VOL_TERPENES_PUNGENT (1u << 4)
#define KOCHEN_VOL_SULFIDES_ALLIUM  (1u << 5)
#define KOCHEN_VOL_ISOTHIOCYANATES  (1u << 6)
#define KOCHEN_VOL_PHENOLS_SPICE    (1u << 7)
#define KOCHEN_VOL_PHENOLS_SMOKE    (1u << 8)
#define KOCHEN_VOL_ALDEHYDES_GREEN  (1u << 9)
#define KOCHEN_VOL_ESTERS_FRUITY    (1u << 10)
#define KOCHEN_VOL_FURANS_CARAMEL   (1u << 11)
#define KOCHEN_VOL_ACIDS_CULTURED   (1u << 12)
#define KOCHEN_VOL_MARINE_SAVORY    (1u << 13)
#define KOCHEN_VOL_EARTHY_TERPENOID (1u << 14)
#define KOCHEN_VOL_CAPSAICINOIDS    (1u << 15)

/* Solubility flags */
#define KOCHEN_SOL_WATER            (1u << 0)
#define KOCHEN_SOL_FAT              (1u << 1)
#define KOCHEN_SOL_ALCOHOL          (1u << 2)

/* Active enzyme flags */
#define KOCHEN_ENZ_PROTEASE         (1u << 0)
#define KOCHEN_ENZ_AMYLASE          (1u << 1)
#define KOCHEN_ENZ_ALLIINASE        (1u << 2)
#define KOCHEN_ENZ_CHELATOR         (1u << 3) /* Calcium chelator e.g. sodium citrate */

/* Dietary & pantry-tier flags */
#define KOCHEN_DIET_MEAT            (1u << 0)
#define KOCHEN_DIET_SEAFOOD         (1u << 1)
#define KOCHEN_DIET_DAIRY           (1u << 2)
#define KOCHEN_DIET_EGG             (1u << 3)
#define KOCHEN_DIET_HONEY_GELATIN   (1u << 4)
#define KOCHEN_DIET_GLUTEN          (1u << 5)
#define KOCHEN_DIET_NUT             (1u << 6)
#define KOCHEN_DIET_SOY             (1u << 7)
#define KOCHEN_DIET_ALCOHOL         (1u << 8)
#define KOCHEN_DIET_MODERNIST       (1u << 9)

#define KOCHEN_FILTER_VEGAN (KOCHEN_DIET_MEAT | KOCHEN_DIET_SEAFOOD | KOCHEN_DIET_DAIRY | KOCHEN_DIET_EGG | KOCHEN_DIET_HONEY_GELATIN)
#define KOCHEN_FILTER_VEGETARIAN (KOCHEN_DIET_MEAT | KOCHEN_DIET_SEAFOOD)

/* Archetype physical precondition guard flags */
#define KOCHEN_GUARD_MAILLARD_DRY    (1u << 0)
#define KOCHEN_GUARD_ENZYME_PROTEASE (1u << 1)
#define KOCHEN_GUARD_PECTIN_ACID     (1u << 2)
#define KOCHEN_GUARD_CASEIN_CEIL     (1u << 3)
#define KOCHEN_GUARD_LIPID_SHORTEN   (1u << 4)
#define KOCHEN_GUARD_FOAM_NO_LIPID   (1u << 5)

/* Home Mode unit kinds */
typedef enum {
    KOCHEN_UNIT_GRAM = 0,
    KOCHEN_UNIT_TSP  = 1,
    KOCHEN_UNIT_TBSP = 2,
    KOCHEN_UNIT_CUP  = 3,
    KOCHEN_UNIT_PCS  = 4,
    KOCHEN_UNIT_CLOVE = 5,
    KOCHEN_UNIT_PINCH = 6
} kochen_home_unit_t;

/* Sensory Equilibrium vector indices */
typedef enum {
    KOCHEN_SENS_RICHNESS = 0,
    KOCHEN_SENS_BRIGHTNESS = 1,
    KOCHEN_SENS_UMAMI = 2,
    KOCHEN_SENS_HEAT = 3,
    KOCHEN_SENS_SWEET_BITTER = 4,
    KOCHEN_SENS_CRUNCH = 5,
    KOCHEN_SENS_COUNT = 6
} kochen_sensory_axis_t;

typedef struct {
    uint16_t id;
    char slug[KOCHEN_SLUG_LEN];

    float water_pct;
    float fat_pct;
    float fat_sat_pct;
    float smoke_point_c;
    float protein_pct;
    kochen_protein_class_t prot_class;
    float starch_pct;
    float amylose_frac;
    float sugar_pct;
    float reducing_sugar_frac;
    float fiber_pectin_pct;

    float ph;
    float buffer_capacity;
    float nacl_pct;
    float glutamate_mg;
    float nucleotide_mg;
    float surfactant_power;
    float surfactant_hlb;
    float hydrocolloid_power;
    float gel_set_c;
    uint8_t enzyme_mask;
    float enzyme_kill_c;

    float pungency_idx;
    float bitterness_idx;
    float crunch_potential;

    uint8_t solubility_mask;
    uint8_t heat_labile;
    uint32_t volatile_mask;
    uint32_t role_mask;
    uint32_t pantry_mask;
    uint32_t diet_mask;

    kochen_home_unit_t home_unit;
    float grams_per_unit;
} kochen_ingredient_t;

typedef struct {
    uint8_t id;
    char slug[KOCHEN_SLUG_LEN];
    float default_novelty_bias;
} kochen_pantry_t;

typedef enum {
    KOCHEN_STAGE_PREP = 0,
    KOCHEN_STAGE_EARLY_FAT = 1,
    KOCHEN_STAGE_CORE_COOK = 2,
    KOCHEN_STAGE_EMULSIFY_BIND = 3,
    KOCHEN_STAGE_LATE_FINISH = 4
} kochen_stage_t;

typedef struct {
    kochen_role_t role;
    uint8_t optional;
    float min_frac;
    float max_frac;
    float target_frac;
    kochen_stage_t stage;
} kochen_slot_spec_t;

typedef struct {
    uint16_t id;
    char slug[KOCHEN_SLUG_LEN];
    uint8_t is_companion_capable;
    uint32_t companion_allow_mask;

    uint8_t slot_count;
    kochen_slot_spec_t slots[KOCHEN_MAX_ARCH_SLOTS];

    float min_hydration_ratio;
    float max_hydration_ratio;
    float min_dispersed_frac;
    float max_dispersed_frac;
    float min_surfactant_ratio;
    float min_gel_protein_pct;
    float max_gel_protein_pct;
    float min_ph;
    float max_ph;
    float target_temp_c;
    float ceiling_temp_c;
    float cook_duration_sec;
    float evap_loss_frac;
    uint32_t guard_flags;
    float default_sensory[KOCHEN_SENS_COUNT];
} kochen_archetype_t;

typedef struct {
    char key[KOCHEN_SLUG_LEN + 16];
    char text[KOCHEN_LANG_COUNT][KOCHEN_STEP_TEXT_LEN];
} kochen_locale_entry_t;

typedef struct {
    uint16_t ingredient_count;
    kochen_ingredient_t ingredients[KOCHEN_MAX_INGREDIENTS];

    uint8_t pantry_count;
    kochen_pantry_t pantries[KOCHEN_MAX_PANTRIES];

    uint8_t archetype_count;
    kochen_archetype_t archetypes[KOCHEN_MAX_ARCHETYPES];

    uint16_t locale_count;
    kochen_locale_entry_t locales[KOCHEN_MAX_LOCALE_KEYS];
} kochen_db_t;

typedef struct {
    uint8_t component_idx; /* 0 = primary, 1 = companion */
    uint8_t arch_slot_idx;
    kochen_role_t role;
    uint8_t locked;
    int16_t ingredient_id; /* -1 if unfilled optional slot */
    float mass_g;
} kochen_patch_slot_t;

typedef struct {
    uint8_t primary_arch_id;
    int8_t companion_arch_id; /* -1 if none */
    uint8_t pantry_id;
    kochen_mode_t solve_mode; /* Mode active when last solved/mutated */
    uint32_t forbidden_diet_mask;
    float novelty;            /* 0.0 (strict regional) .. 1.0 (unconstrained volatile bridge) */
    float target_sensory[KOCHEN_SENS_COUNT];
    float batch_mass_g;       /* Default 500g primary (+ 160g companion if active) */
    uint32_t seed;

    uint8_t slot_count;
    kochen_patch_slot_t slots[KOCHEN_MAX_PATCH_SLOTS];
} kochen_patch_t;

typedef struct {
    float total_mass_g;
    float final_mass_g;
    float water_g;
    float fat_g;
    float protein_g;
    float starch_g;
    float sugar_g;
    float nacl_pct;
    float mixture_ph;
    float hydration_ratio;
    float dispersed_fat_frac;
    float surfactant_coverage; /* >= 1.0 means stable emulsion when required */
    float mean_hlb;
    float gel_protein_pct;
    float umami_synergy;
    float sensory[KOCHEN_SENS_COUNT];

    bool emulsion_stable;
    bool ph_in_window;
    bool hydration_in_window;
    bool gel_in_window;
    bool protease_hazard_raw;      /* True before guard, mitigated by OP_PRE_SCALD_ENZYME */
    bool pectin_acid_hazard_raw;   /* Mitigated by delayed acid node */
    bool casein_temp_capped;       /* True when <68C ceiling enforced */
    bool has_modernist_additive;   /* Triggers explicit Home Pantry re-solve badge in Home view */
    uint32_t shared_volatile_mask; /* Union of volatile bridges present across >=2 slots */
} kochen_diagnostics_t;

typedef enum {
    KOCHEN_OP_PAT_DRY_BRINE = 0,
    KOCHEN_OP_PRE_SCALD_ENZYME = 1,
    KOCHEN_OP_HYDRATE_NEUTRAL = 2,
    KOCHEN_OP_LIPID_SHORTEN_KNEAD = 3,
    KOCHEN_OP_WHIP_CLEAN_FOAM = 4,
    KOCHEN_OP_BLOOM_FAT_AROMA = 5,
    KOCHEN_OP_MAILLARD_SEAR = 6,
    KOCHEN_OP_STARCH_COOK = 7,
    KOCHEN_OP_COLLAGEN_BRAISE = 8,
    KOCHEN_OP_SHEAR_EMULSIFY = 9,
    KOCHEN_OP_GENTLE_COAGULATE = 10,
    KOCHEN_OP_SUCROSE_STAGE = 11,
    KOCHEN_OP_ACID_OSMOTIC_CURE = 12,
    KOCHEN_OP_CRUST_DEHYDRATE_FRY = 13,
    KOCHEN_OP_POST_SOFTEN_ACID = 14,
    KOCHEN_OP_OFF_HEAT_VOLATILES = 15,
    KOCHEN_OP_PLATE_ASSEMBLE = 16
} kochen_op_kind_t;

typedef struct {
    uint8_t node_id;
    uint8_t component_idx; /* 0 = primary, 1 = companion, 2 = merged assembly */
    kochen_op_kind_t op;
    uint8_t parent_count;
    uint8_t parents[4];
    uint8_t slot_ref_count;
    uint8_t slot_refs[8];
    float temp_c;
    float duration_sec;
    float out_mass_g;
    float out_ph;
} kochen_dag_node_t;

typedef struct {
    uint8_t node_count;
    kochen_dag_node_t nodes[KOCHEN_MAX_DAG_NODES];
    uint32_t active_guards;
    kochen_diagnostics_t primary_diag;
    kochen_diagnostics_t companion_diag;
    kochen_diagnostics_t total_diag;
} kochen_dag_t;

typedef struct {
    char name[KOCHEN_NAME_LEN];
    char role_label[KOCHEN_NAME_LEN];
    char amount_text[64];
    uint8_t component_idx;
    uint8_t locked;
    uint8_t is_modernist;
    uint32_t bridge_mask;
} kochen_projected_slot_t;

typedef struct {
    uint8_t node_id;
    uint8_t component_idx;
    char title[KOCHEN_NAME_LEN];
    char metrics_badge[96];
    char instruction[KOCHEN_STEP_TEXT_LEN];
} kochen_projected_step_t;

typedef struct {
    kochen_mode_t view_mode;
    kochen_lang_t lang;
    char dish_title[128];
    char subtitle[128];
    bool show_home_resolve_badge;
    char home_resolve_prompt[160];

    uint8_t slot_count;
    kochen_projected_slot_t slots[KOCHEN_MAX_PATCH_SLOTS];

    uint8_t step_count;
    kochen_projected_step_t steps[KOCHEN_MAX_DAG_NODES];
} kochen_projection_t;

typedef enum {
    KOCHEN_OK = 0,
    KOCHEN_ERR_IO = -1,
    KOCHEN_ERR_PARSE = -2,
    KOCHEN_ERR_RANGE = -3,
    KOCHEN_ERR_CAPACITY = -4,
    KOCHEN_ERR_INFEASIBLE = -5,
    KOCHEN_ERR_CHECKSUM = -6
} kochen_status_t;

#endif /* KOCHEN_TYPES_H */
