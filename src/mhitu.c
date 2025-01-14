/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "artifact.h"

static struct obj *mon_currwep = (struct obj *) 0;

static void urustm(struct monst *, struct obj *);
static boolean u_slip_free(struct monst *, struct attack *);
static int passiveum(struct permonst *, struct monst *, struct attack *);

# ifdef SEDUCE
static void mayberem(struct monst *, const char *, struct obj *, const char *);
# endif

static boolean diseasemu(struct permonst *);
static int hitmu(struct monst *, struct attack *);
static int gulpmu(struct monst *, struct attack *);
static int explmu(struct monst *, struct attack *, boolean);
static void missmu(struct monst *, boolean, struct attack *);
static void mswings(struct monst *, struct obj *);
static void wildmiss(struct monst *, struct attack *);
static int mon_scream(struct monst*, struct attack*);

static void hitmsg(struct monst *, struct attack *);

static void invulnerability_messages(struct monst *, boolean, boolean);

/* See comment in mhitm.c.  If we use this a lot it probably should be */
/* changed to a parameter to mhitu. */
static int dieroll;

static void
hitmsg(struct monst *mtmp, struct attack *mattk)
{
    int compat;

    /* Note: if opposite gender, "seductively" */
    /* If same gender, "engagingly" for nymph, normal msg for others */
    if ((compat = could_seduce(mtmp, &youmonst, mattk)) &&
        (!mtmp->mcan && !mtmp->mspec_used)) {
        pline("%s %s you %s.", Monnam(mtmp),
              Blind ? "talks to" : "smiles at",
              compat == 2 ? "engagingly" : "seductively");
    } else
        switch (mattk->aatyp) {
        case AT_BITE:
            pline("%s bites!", Monnam(mtmp));
            break;

        case AT_KICK:
            pline("%s kicks%c", Monnam(mtmp),
                  thick_skinned(youmonst.data) ? '.' : '!');
            break;

        case AT_STNG:
            pline("%s stings!", Monnam(mtmp));
            break;

        case AT_BUTT:
            pline("%s butts!", Monnam(mtmp));
            break;

        case AT_TUCH:
            pline("%s touches you!", Monnam(mtmp));
            break;

        case AT_TENT:
            pline("%s tentacles suck you!",
                  s_suffix(Monnam(mtmp)));
            break;

        case AT_EXPL:
        case AT_BOOM:
            pline("%s explodes!", Monnam(mtmp));
            break;

        default:
            pline("%s hits!", Monnam(mtmp));
        }
}

/* monster missed you */
static void
missmu(struct monst *mtmp, boolean nearmiss, struct attack *mattk)
{
    if (!canspotmon(mtmp))
        map_invisible(mtmp->mx, mtmp->my);

    if(could_seduce(mtmp, &youmonst, mattk) && !mtmp->mcan)
        pline("%s pretends to be friendly.", Monnam(mtmp));
    else {
        if (!flags.verbose || !nearmiss)
            pline("%s misses.", Monnam(mtmp));
        else
            pline("%s just misses!", Monnam(mtmp));
    }
    stop_occupation();
}

static void
mswings(struct monst *mtmp, struct obj *otemp)        /* monster swings obj */


{
    if (!flags.verbose || Blind || !mon_visible(mtmp))
        return;
    pline("%s %s %s %s.", Monnam(mtmp),
          (objects[otemp->otyp].oc_dir & PIERCE) ? "thrusts" : "swings",
          mhis(mtmp), singular(otemp, xname));
}

/* return how a poison attack was delivered */
const char *
mpoisons_subj(struct monst *mtmp, struct attack *mattk)
{
    if (mattk->aatyp == AT_WEAP) {
        struct obj *mwep = (mtmp == &youmonst) ? uwep : MON_WEP(mtmp);
        /* "Foo's attack was poisoned." is pretty lame, but at least
           it's better than "sting" when not a stinging attack... */
        return (!mwep || !mwep->opoisoned) ? "attack" : "weapon";
    } else {
        return (mattk->aatyp == AT_TUCH) ? "contact" :
               (mattk->aatyp == AT_GAZE) ? "gaze" :
               (mattk->aatyp == AT_BITE) ? "bite" : "sting";
    }
}

/* called when your intrinsic speed is taken away */
void
u_slow_down(void)
{
    HFast = 0L;
    if (!Fast)
        You("slow down.");
    else    /* speed boots */
        Your("quickness feels less natural.");
    exercise(A_DEX, FALSE);
}

static void
wildmiss(struct monst *mtmp, struct attack *mattk)       /* monster attacked your displaced image */


{
    int compat;

    /* no map_invisible() -- no way to tell where _this_ is coming from */

    if (!flags.verbose) return;
    if (!cansee(mtmp->mx, mtmp->my)) return;
    /* maybe it's attacking an image around the corner? */

    compat = (mattk->adtyp == AD_SEDU || mattk->adtyp == AD_SSEX) &&
             could_seduce(mtmp, &youmonst, (struct attack *)0);

    if (!mtmp->mcansee || (Invis && !perceives(mtmp->data))) {
        const char *swings =
            mattk->aatyp == AT_BITE ? "snaps" :
            mattk->aatyp == AT_KICK ? "kicks" :
            (mattk->aatyp == AT_STNG ||
             mattk->aatyp == AT_BUTT ||
             nolimbs(mtmp->data)) ? "lunges" : "swings";

        if (compat)
            pline("%s tries to touch you and misses!", Monnam(mtmp));
        else
            switch (rn2(3)) {
            case 0: pline("%s %s wildly and misses!", Monnam(mtmp),
                          swings);
                break;

            case 1: pline("%s attacks a spot beside you.", Monnam(mtmp));
                break;

            case 2: pline("%s strikes at %s!", Monnam(mtmp),
                          levl[mtmp->mux][mtmp->muy].typ == WATER
                          ? "empty water" : "thin air");
                break;

            default: pline("%s %s wildly!", Monnam(mtmp), swings);
                break;
            }

    } else if (Displaced) {
        /* give 'displaced' message even if hero is Blind */
        if (compat)
            pline("%s smiles %s at your %sdisplaced image...",
                  Monnam(mtmp),
                  compat == 2 ? "engagingly" : "seductively",
                  Invis ? "invisible " : "");
        else
            pline("%s strikes at your %sdisplaced image and misses you!",
                  /* Note: if you're both invisible and displaced,
                   * only monsters which see invisible will attack your
                   * displaced image, since the displaced image is also
                   * invisible.
                   */
                  Monnam(mtmp),
                  Invis ? "invisible " : "");

    } else if (Underwater) {
        /* monsters may miss especially on water level where
           bubbles shake the player here and there */
        if (compat)
            pline("%s reaches towards your distorted image.", Monnam(mtmp));
        else
            pline("%s is fooled by water reflections and misses!", Monnam(mtmp));

    } else warning("%s attacks you without knowing your location?",
                   Monnam(mtmp));
}

void
expels(struct monst *mtmp, struct permonst *mdat, boolean message)

                       /* if mtmp is polymorphed, mdat != mtmp->data */

{
    if (message) {
        if (is_animal(mdat))
            You("get regurgitated!");
        else {
            char blast[40];
            struct attack *attk = attacktype_fordmg(mdat, AT_ENGL, AD_ANY);

            blast[0] = '\0';
            if (!attk) {
                warning("Swallower has no engulfing attack?");
            } else {
                if (is_whirly(mdat)) {
                    switch (attk->adtyp) {
                    case AD_ELEC:
                        Strcpy(blast,
                               " in a shower of sparks");
                        break;

                    case AD_COLD:
                        Strcpy(blast,
                               " in a blast of frost");
                        break;
                    }
                } else
                    Strcpy(blast, " with a squelch");
                You("get expelled from %s%s!",
                    mon_nam(mtmp), blast);
            }
        }
    }
    unstuck(mtmp); /* ball&chain returned in unstuck() */
    mnexto(mtmp);
    newsym(u.ux, u.uy);
    spoteffects(TRUE);
    /* to cover for a case where mtmp is not in a next square */
    if(um_dist(mtmp->mx, mtmp->my, 1))
        pline("Brrooaa...  You land hard at some distance.");
}

/* select a monster's next attack, possibly substituting for its usual one */
struct attack *
getmattk(struct monst *magr, struct monst *mdef, int indx, int *prev_result, struct attack *alt_attk_buf)
{
    struct permonst *mptr = magr->data;
    struct attack *attk = &mptr->mattk[indx];
    struct obj *weap = (magr == &youmonst) ? uwep : MON_WEP(magr);

    /* honor SEDUCE=0 */
#ifdef SEDUCE
    extern const struct attack sa_no[NATTK];

    /* if the first attack is for SSEX damage, all six attacks will be
        substituted (expected succubus/incubus handling); if it isn't
        but another one is, only that other one will be substituted */
    if (mptr->mattk[0].adtyp == AD_SSEX) {
        *alt_attk_buf = sa_no[indx];
        attk = alt_attk_buf;
    } else if (attk->adtyp == AD_SSEX) {
        *alt_attk_buf = *attk;
        attk = alt_attk_buf;
        attk->adtyp = AD_DRLI;
    }
#endif

    /* prevent a monster with two consecutive disease or hunger attacks
       from hitting with both of them on the same turn; if the first has
       already hit, switch to a stun attack for the second */
    if (indx > 0 && prev_result[indx - 1] > 0 &&
        (attk->adtyp == AD_DISE ||
         attk->adtyp == AD_PEST ||
         attk->adtyp == AD_FAMN) &&
        attk->adtyp == mptr->mattk[indx - 1].adtyp) {
        *alt_attk_buf = *attk;
        attk = alt_attk_buf;
        attk->adtyp = AD_STUN;
    } else if (attk->aatyp == AT_ENGL && magr->mspec_used) {
        /* can't re-engulf yet; switch to simpler attack */
        *alt_attk_buf = *attk;
        attk = alt_attk_buf;
        if (attk->adtyp == AD_ACID ||
            attk->adtyp == AD_ELEC ||
            attk->adtyp == AD_COLD ||
            attk->adtyp == AD_FIRE) {
            attk->aatyp = AT_TUCH;
        } else {
            attk->aatyp = AT_CLAW; /* attack message will be "<foo> hits" */
            attk->adtyp = AD_PHYS;
        }
        attk->damn = 1; /* relatively weak: 1d6 */
        attk->damd = 6;

    /* barrow wight, Nazgul, erinys have weapon attack for non-physical
       damage; force physical damage if attacker has been cancelled or
       if weapon is sufficiently interesting; a few unique creatures
       have two weapon attacks where one does physical damage and other
       doesn't--avoid forcing physical damage for those */
    } else if (indx == 0 &&
               magr != &youmonst &&
               attk->aatyp == AT_WEAP &&
               attk->adtyp != AD_PHYS &&
               !(mptr->mattk[1].aatyp == AT_WEAP && mptr->mattk[1].adtyp == AD_PHYS) &&
               (magr->mcan ||
                (weap &&
                 ((weap->otyp == CORPSE && touch_petrifies(&mons[weap->corpsenm])) ||
                 (weap->oartifact == ART_STORMBRINGER ||
                  weap->oartifact == ART_VORPAL_BLADE))))) {
        *alt_attk_buf = *attk;
        attk = alt_attk_buf;
        attk->adtyp = AD_PHYS;

    /* make drain-energy damage be somewhat in proportion to energy */
    } else if (attk->adtyp == AD_DREN && mdef == &youmonst) {
        int ulev = max(u.ulevel, 6);

        *alt_attk_buf = *attk;
        attk = alt_attk_buf;
        /* 3.6.0 used 4d6 but since energy drain came out of max energy
           once current energy was gone, that tended to have a severe
           effect on low energy characters; it's now 2d6 with ajustments */
        if (u.uen <= 5 * ulev && attk->damn > 1) {
            attk->damn -= 1; /* low energy: 2d6 -> 1d6 */
            if (u.uenmax <= 2 * ulev && attk->damd > 3) {
                attk->damd -= 3; /* very low energy: 1d6 -> 1d3 */
            }
        } else if (u.uen > 12 * ulev) {
            attk->damn += 1; /* high energy: 2d6 -> 3d6 */
            if (u.uenmax > 20 * ulev) {
                attk->damd += 3; /* very high energy: 3d6 -> 3d9 */
            }
            /* note: 3d9 is slightly higher than previous 4d6 */
        }
    }

    return attk;
}

/*
 * mattacku: monster attacks you
 *  returns 1 if monster dies (e.g. "yellow light"), 0 otherwise
 *  Note: if you're displaced or invisible the monster might attack the
 *      wrong position...
 *  Assumption: it's attacking you or an empty square; if there's another
 *      monster which it attacks by mistake, the caller had better
 *      take care of it...
 */
int
mattacku(struct monst *mtmp)
{
    struct  attack  *mattk, alt_attk;
    int i, j=0, tmp, sum[NATTK];
    struct  permonst *mdat = mtmp->data;
    boolean ranged = (distu(mtmp->mx, mtmp->my) > 3);
    /* Is it near you?  Affects your actions */
    boolean range2 = !monnear(mtmp, mtmp->mux, mtmp->muy);
    /* Does it think it's near you?  Affects its actions */
    boolean foundyou = (mtmp->mux==u.ux && mtmp->muy==u.uy);
    /* Is it attacking you or your image? */
    boolean youseeit = canseemon(mtmp);
    /* Might be attacking your image around the corner, or
     * invisible, or you might be blind....
     */
    boolean skipnonmagc = FALSE;
    /* Are further physical attack attempts useless? */

    if(!ranged) nomul(0, 0);
    if(mtmp->mhp <= 0 || (Underwater && !is_swimmer(mtmp->data)))
        return(0);

    /* If swallowed, can only be affected by u.ustuck */
    if(u.uswallow) {
        if(mtmp != u.ustuck)
            return(0);
        u.ustuck->mux = u.ux;
        u.ustuck->muy = u.uy;
        range2 = 0;
        foundyou = 1;
        if(u.uinvulnerable) return (0); /* stomachs can't hurt you! */
    }

    else if (u.usteed) {
        if (mtmp == u.usteed)
            /* Your steed won't attack you */
            return (0);
        /* Orcs like to steal and eat horses and the like */
        if (!rn2(is_orc(mtmp->data) ? 2 : 4) &&
            distu(mtmp->mx, mtmp->my) <= 2) {
            /* Attack your steed instead */
            i = mattackm(mtmp, u.usteed);
            if ((i & MM_AGR_DIED))
                return (1);
            /* make sure steed is still alive and within range */
            if ((i & MM_DEF_DIED) || !u.usteed || distu(mtmp->mx, mtmp->my) > 2) {
                return 0;
            }
            /* Let your steed retaliate */
            return (!!(mattackm(u.usteed, mtmp) & MM_DEF_DIED));
        }
    }

    if (u.uundetected && !range2 && foundyou && !u.uswallow) {
        if (!canspotmon(mtmp)) {
            map_invisible(mtmp->mx, mtmp->my);
        }
        u.uundetected = 0;
        if (is_hider(youmonst.data) && u.umonnum != PM_TRAPPER) {
            /* ceiling hider */
            coord cc; /* maybe we need a unexto() function? */
            struct obj *obj;

            You("fall from the %s!", ceiling(u.ux, u.uy));
            /* take monster off map now so that its location
               is eligible for placing hero; we assume that a
               removed monster remembers its old spot <mx,my> */
            remove_monster(mtmp->mx, mtmp->my);
            if (!enexto(&cc, u.ux, u.uy, youmonst.data) ||
                /* a fish won't voluntarily swap positions
                   when it's in water and hero is over land */
                 (mtmp->data->mlet == S_EEL && is_pool(mtmp->mx, mtmp->my) &&
                  (!is_pool(u.ux, u.uy)))) {
                /* couldn't find any spot for hero; this used to
                   kill off attacker, but now we just give a "miss"
                   message and keep both mtmp and hero at their
                   original positions; hero has become unconcealed
                   so mtmp's next move will be a regular attack */
                place_monster(mtmp, mtmp->mx, mtmp->my); /* put back */
                newsym(u.ux, u.uy); /* u.uundetected was toggled */
                pline("%s draws back as you drop!", Monnam(mtmp));
                return 0;
            }

            /* put mtmp at hero's spot and move hero to <cc.x,.y> */
            newsym(mtmp->mx, mtmp->my); /* finish removal */
            place_monster(mtmp, u.ux, u.uy);
            if (mtmp->wormno) {
                worm_move(mtmp);
                /* tail hasn't grown, so if it now occupies <cc.x,.y>
                   then one of its original spots must be free */
                if (m_at(cc.x, cc.y)) {
                    (void) enexto(&cc, u.ux, u.uy, youmonst.data);
                }
            }
            teleds(cc.x, cc.y, TRUE); /* move hero */
            set_apparxy(mtmp);
            newsym(u.ux, u.uy);

            if (youmonst.data->mlet != S_PIERCER)
                return(0); /* trappers don't attack */

            obj = which_armor(mtmp, WORN_HELMET);
            if (obj && is_metallic(obj)) {
                Your("blow glances off %s %s.", s_suffix(mon_nam(mtmp)), helm_simple_name(obj));
            } else {
                if (3 + find_mac(mtmp) <= rnd(20)) {
                    pline("%s is hit by a falling piercer (you)!",
                          Monnam(mtmp));
                    if ((mtmp->mhp -= d(3, 6)) < 1)
                        killed(mtmp);
                } else
                    pline("%s is almost hit by a falling piercer (you)!",
                          Monnam(mtmp));
            }
        } else {
            /* surface hider */
            if (!youseeit) {
                pline("It tries to move where you are hiding.");
            } else {
                /* Ugly kludge for eggs.  The message is phrased so as
                 * to be directed at the monster, not the player,
                 * which makes "laid by you" wrong.  For the
                 * parallelism to work, we can't rephrase it, so we
                 * zap the "laid by you" momentarily instead.
                 */
                struct obj *obj = level.objects[u.ux][u.uy];

                if (obj || u.umonnum == PM_TRAPPER ||
                    (youmonst.data->mlet == S_EEL && is_pool(u.ux, u.uy))) {
                    int save_spe = 0; /* suppress warning */

                    if (obj) {
                        save_spe = obj->spe;
                        if (obj->otyp == EGG) obj->spe = 0;
                    }
                    /* note that m_monnam() overrides hallucination, which is
                       what we want when message is from mtmp's perspective */
                    if (youmonst.data->mlet == S_EEL || u.umonnum == PM_TRAPPER) {
                        pline("Wait, %s!  There's a hidden %s named %s there!",
                              m_monnam(mtmp), youmonst.data->mname, plname);
                    } else {
                        pline("Wait, %s!  There's a %s named %s hiding under %s!",
                              m_monnam(mtmp), youmonst.data->mname, plname,
                              doname(level.objects[u.ux][u.uy]));
                    }
                    if (obj) obj->spe = save_spe;
                } else
                    warning("hiding under nothing?");
            }
            newsym(u.ux, u.uy);
        }
        return(0);
    }

    /* hero might be a mimic, concealed via #monster */
    if (youmonst.data->mlet == S_MIMIC && U_AP_TYPE && !range2 && foundyou && !u.uswallow) {
        boolean sticky = sticks(youmonst.data);

        if (!canspotmon(mtmp)) {
            map_invisible(mtmp->mx, mtmp->my);
        }
        if (sticky && !youseeit) {
            pline("It gets stuck on you.");
        } else {
            /* see note about m_monnam() above */
            pline("Wait, %s!  That's a %s named %s!", m_monnam(mtmp),
                  youmonst.data->mname, plname);
        }
        if (sticky) {
            u.ustuck = mtmp;
        }
        youmonst.m_ap_type = M_AP_NOTHING;
        youmonst.mappearance = 0;
        newsym(u.ux, u.uy);
        return(0);
    }

    /* non-mimic hero might be mimicking an object after eating m corpse */
    if (U_AP_TYPE == M_AP_OBJECT && !range2 && foundyou && !u.uswallow) {
        if (!canspotmon(mtmp)) {
            map_invisible(mtmp->mx, mtmp->my);
        }
        if (!youseeit)
            pline("%s %s!", Something,
                  (likes_gold(mtmp->data) && youmonst.mappearance == GOLD_PIECE) ?
                  "tries to pick you up" : "disturbs you");
        else pline("Wait, %s!  That %s is really %s named %s!",
                   m_monnam(mtmp),
                   mimic_obj_name(&youmonst),
                   an(mons[u.umonnum].mname),
                   plname);
        if (multi < 0) {    /* this should always be the case */
            char buf[BUFSZ];
            Sprintf(buf, "You appear to be %s again.",
                    Upolyd ? (const char *) an(youmonst.data->mname) :
                    (const char *) "yourself");
            unmul(buf); /* immediately stop mimicking */
        }
        return 0;
    }

    /*  Work out the armor class differential   */
    tmp = AC_VALUE(u.uac) + 10;     /* tmp ~= 0 - 20 */
    tmp += mtmp->m_lev;
    if(multi < 0) tmp += 4;
    if((Invis && !perceives(mdat)) || !mtmp->mcansee)
        tmp -= 2;
    if(mtmp->mtrapped) tmp -= 2;
    if(tmp <= 0) tmp = 1;

    /* make eels visible the moment they hit/miss us */
    if(mdat->mlet == S_EEL && mtmp->minvis && cansee(mtmp->mx, mtmp->my)) {
        mtmp->minvis = 0;
        newsym(mtmp->mx, mtmp->my);
    }

    /* Special demon handling code */
    if(!mtmp->cham && is_demon(mdat) && !range2
       && mtmp->data != &mons[PM_BALROG]
       && mtmp->data != &mons[PM_SUCCUBUS]
       && mtmp->data != &mons[PM_INCUBUS]
       && (!uwep || uwep->oartifact != ART_DEMONBANE))
        if(!mtmp->mcan && !rn2(13)) msummon(mtmp);

    /* Special lycanthrope handling code */
    if ((mtmp->cham == NON_PM) && is_were(mdat) && !range2) {

        if (is_human(mdat)) {
            if(!rn2(5 - (night() * 2)) && !mtmp->mcan) new_were(mtmp);
        } else if(!rn2(30) && !mtmp->mcan) new_were(mtmp);
        mdat = mtmp->data;

        if (!rn2(10) && !mtmp->mcan) {
            int numseen, numhelp;
            char buf[BUFSZ], genericwere[BUFSZ];

            Strcpy(genericwere, "creature");
            numhelp = were_summon(mdat, FALSE, &numseen, genericwere);
            if (youseeit) {
                pline("%s summons help!", Monnam(mtmp));
                if (numhelp > 0) {
                    if (numseen == 0)
                        You_feel("hemmed in.");
                } else pline("But none comes.");
            } else {
                const char *from_nowhere;

                if (!Deaf) {
                    pline("%s %s!", Something,
                          makeplural(growl_sound(mtmp)));
                    from_nowhere = "";
                } else from_nowhere = " from nowhere";
                if (numhelp > 0) {
                    if (numseen < 1) You_feel("hemmed in.");
                    else {
                        if (numseen == 1)
                            Sprintf(buf, "%s appears",
                                    an(genericwere));
                        else
                            Sprintf(buf, "%s appear",
                                    makeplural(genericwere));
                        pline("%s%s!", upstart(buf), from_nowhere);
                    }
                } /* else no help came; but you didn't know it tried */
            }
        }
    }

    if (u.uinvulnerable) {
        invulnerability_messages(mtmp, range2, youseeit);
        return (0);
    }

    /* Unlike defensive stuff, don't let them use item _and_ attack. */
    if (find_offensive(mtmp)) {
        int foo = use_offensive(mtmp);

        if (foo != 0) return(foo==1);
    }

    for (i = 0; i < NATTK; i++) {
        /* invulnerability may occur between attacks
           (when HoH life saving.). Cancel attacks if we become
           invulnerable. */
        if (u.uinvulnerable)
        {
            invulnerability_messages(mtmp, range2, youseeit);
            break;
        }

        sum[i] = 0;
        mon_currwep = (struct obj *)0;
        mattk = getmattk(mtmp, &youmonst, i, sum, &alt_attk);
        if (u.uswallow && (mattk->aatyp != AT_ENGL))
        if ((u.uswallow && mattk->aatyp != AT_ENGL) ||
            (skipnonmagc && mattk->aatyp != AT_MAGC)) {
            continue;
        }
        switch (mattk->aatyp) {
        case AT_CLAW: /* "hand to hand" attacks */
        case AT_KICK:
        case AT_BITE:
        case AT_STNG:
        case AT_TUCH:
        case AT_BUTT:
        case AT_TENT:
            if (!range2 && (!MON_WEP(mtmp) || mtmp->mconf || Conflict ||
                           !touch_petrifies(youmonst.data))) {
                if (foundyou) {
                    if (tmp > (j = rnd(20+i))) {
                        if (mattk->aatyp != AT_KICK ||
                            !thick_skinned(youmonst.data))
                            sum[i] = hitmu(mtmp, mattk);
                    } else
                        missmu(mtmp, (tmp == j), mattk);
                } else {
                    wildmiss(mtmp, mattk);
                    /* skip any remaining non-spell attacks */
                    skipnonmagc = TRUE;
                }
            }
            break;

        case AT_HUGS: /* automatic if prev two attacks succeed */
            /* Notes: also automatic if attacker is a plant;
               if displaced, prev attacks never succeeded */
            if ((!range2 && ((i>=2 && sum[i-1] && sum[i-2]) ||
                            is_vegetation(mdat))) || mtmp == u.ustuck)
                sum[i]= hitmu(mtmp, mattk);
            break;

        case AT_GAZE: /* can affect you either ranged or not */
            /* Medusa gaze already operated through m_respond in
             * dochug(); don't gaze more than once per round.
             */
            if (mdat != &mons[PM_MEDUSA])
                sum[i] = gazemu(mtmp, mattk);
            break;

        case AT_EXPL:   /* automatic hit if next to, and aimed at you */
            if(!range2) sum[i] = explmu(mtmp, mattk, foundyou);
            if (is_fern_spore(mdat)) spore_dies(mtmp);
            break;

        case AT_ENGL:
            if (!range2) {
                if (foundyou) {
                    if (u.uswallow ||
                        (!mtmp->mspec_used && tmp > (j = rnd(20+i)))) {
                        /* Force swallowing monster to be
                         * displayed even when player is
                         * moving away */
                        flush_screen(1);
                        sum[i] = gulpmu(mtmp, mattk);
                    } else {
                        missmu(mtmp, (tmp == j), mattk);
                    }
                } else if (is_animal(mtmp->data)) {
                    pline("%s gulps some air!", Monnam(mtmp));
                } else {
                    if (youseeit)
                        pline("%s lunges forward and recoils!",
                              Monnam(mtmp));
                    else
                        You_hear("a %s nearby.",
                                 is_whirly(mtmp->data) ?
                                 "rushing noise" : "splat");
                }
            }
            break;

        case AT_BREA:
            if(range2) sum[i] = breamu(mtmp, mattk);
            /* Note: breamu takes care of displacement */
            break;

        case AT_SPIT:
            if(range2) sum[i] = spitmu(mtmp, mattk);
            /* Note: spitmu takes care of displacement */
            break;

        case AT_WEAP:
            if (range2) {
#ifdef REINCARNATION
                if (!Is_rogue_level(&u.uz))
#endif
                thrwmu(mtmp);
            } else {
                int hittmp = 0;

                /* Rare but not impossible.  Normally the monster
                 * wields when 2 spaces away, but it can be
                 * teleported or whatever....
                 */
                if (mtmp->weapon_check == NEED_WEAPON ||
                    !MON_WEP(mtmp)) {
                    mtmp->weapon_check = NEED_HTH_WEAPON;
                    /* mon_wield_item resets weapon_check as
                     * appropriate */
                    if (mon_wield_item(mtmp) != 0) break;
                }
                if (foundyou) {
                    mon_currwep = MON_WEP(mtmp);
                    if (mon_currwep) {
                        hittmp = hitval(mon_currwep, &youmonst);
                        tmp += hittmp;
                        mswings(mtmp, mon_currwep);
                    }
                    if(tmp > (j = dieroll = rnd(20+i)))
                        sum[i] = hitmu(mtmp, mattk);
                    else
                        missmu(mtmp, (tmp == j), mattk);
                    /* KMH -- Don't accumulate to-hit bonuses */
                    if (mon_currwep) {
                        tmp -= hittmp;
                    }
                } else {
                    wildmiss(mtmp, mattk);
                    /* skip any remaining non-spell attacks */
                    skipnonmagc = TRUE;
                }
            }
            break;

        case AT_MAGC:
            if (range2)
                sum[i] = buzzmu(mtmp, mattk);
            else {
                if (foundyou)
                    sum[i] = castmu(mtmp, mattk, TRUE, TRUE);
                else
                    sum[i] = castmu(mtmp, mattk, TRUE, FALSE);
            }
            break;

        case AT_SCRE:
            if (ranged) {
                sum[i] = mon_scream(mtmp, mattk);
            }
            /* if you're nice and close, don't bother */
            break;

        default:        /* no attack */
            break;
        }
        if(flags.botl) bot();
        /* give player a chance of waking up before dying -kaa */
        if(sum[i] == 1) {       /* successful attack */
            if (u.usleep && u.usleep < monstermoves && !rn2(10)) {
                multi = -1;
                nomovemsg = "The combat suddenly awakens you.";
            }
        }
        if(sum[i] == 2) return 1;       /* attacker dead */
        if(sum[i] == 3) break;  /* attacker teleported, no more attacks */
        /* sum[i] == 0: unsuccessful attack */
    }
    return(0);
}

static boolean
diseasemu(struct permonst *mdat)
{
    if (Sick_resistance) {
        You_feel("a slight illness.");
        return FALSE;
    } else {
        make_sick(Sick ? Sick/3L + 1L : (long)rn1(ACURR(A_CON), 20),
                  mdat->mname, TRUE, SICK_NONVOMITABLE);
        return TRUE;
    }
}

/* check whether slippery clothing protects from hug or wrap attack */
static boolean
u_slip_free(struct monst *mtmp, struct attack *mattk)
{
    struct obj *obj = (uarmc ? uarmc : uarm);

    if (!obj) obj = uarmu;
    if (mattk->adtyp == AD_DRIN) obj = uarmh;

    /* if your cloak/armor is greased, monster slips off; this
       protection might fail (33% chance) when the armor is cursed */
    if (obj && (obj->greased || obj->otyp == OILSKIN_CLOAK) &&
        (!obj->cursed || rn2(3))) {
        pline("%s %s your %s %s!",
              Monnam(mtmp),
              (mattk->adtyp == AD_WRAP) ?
              "slips off of" : "grabs you, but cannot hold onto",
              obj->greased ? "greased" : "slippery",
              /* avoid "slippery slippery cloak"
                 for undiscovered oilskin cloak */
              (obj->greased || objects[obj->otyp].oc_name_known) ?
              xname(obj) : cloak_simple_name(obj));

        if (obj->greased && rn2(2)) {
            pline_The("grease wears off.");
            obj->greased = 0;
            update_inventory();
        }
        return TRUE;
    }
    return FALSE;
}

/* armor that sufficiently covers the body might be able to block magic */
int
magic_negation(struct monst *mon)
{
    struct obj *armor;
    int armpro = 0;

    armor = (mon == &youmonst) ? uarm : which_armor(mon, W_ARM);
    if (armor && armpro < objects[armor->otyp].a_can)
        armpro = objects[armor->otyp].a_can;
    armor = (mon == &youmonst) ? uarmc : which_armor(mon, W_ARMC);
    if (armor && armpro < objects[armor->otyp].a_can)
        armpro = objects[armor->otyp].a_can;
    armor = (mon == &youmonst) ? uarmh : which_armor(mon, W_ARMH);
    if (armor && armpro < objects[armor->otyp].a_can)
        armpro = objects[armor->otyp].a_can;

    /* armor types for shirt, gloves, shoes, and shield don't currently
       provide any magic cancellation but we might as well be complete */
    armor = (mon == &youmonst) ? uarmu : which_armor(mon, W_ARMU);
    if (armor && armpro < objects[armor->otyp].a_can)
        armpro = objects[armor->otyp].a_can;
    armor = (mon == &youmonst) ? uarmg : which_armor(mon, W_ARMG);
    if (armor && armpro < objects[armor->otyp].a_can)
        armpro = objects[armor->otyp].a_can;
    armor = (mon == &youmonst) ? uarmf : which_armor(mon, W_ARMF);
    if (armor && armpro < objects[armor->otyp].a_can)
        armpro = objects[armor->otyp].a_can;
    armor = (mon == &youmonst) ? uarms : which_armor(mon, W_ARMS);
    if (armor && armpro < objects[armor->otyp].a_can)
        armpro = objects[armor->otyp].a_can;

    /* this one is really a stretch... */
    armor = (mon == &youmonst) ? 0 : which_armor(mon, W_SADDLE);
    if (armor && armpro < objects[armor->otyp].a_can)
        armpro = objects[armor->otyp].a_can;

    return armpro;
}

/*
 * hitmu: monster hits you
 *    returns 2 if monster dies (e.g. "yellow light"), 1 otherwise
 *    3 if the monster lives but teleported/paralyzed, so it can't keep
 *         attacking you
 */
static int
hitmu(struct monst *mtmp, struct attack *mattk)
{
    struct permonst *mdat = mtmp->data;
    int uncancelled, ptmp;
    int dmg, armpro, permdmg;
    char buf[BUFSZ];
    struct permonst *olduasmon = youmonst.data;
    int res;

    if (!canspotmon(mtmp))
        map_invisible(mtmp->mx, mtmp->my);

/*  If the monster is undetected & hits you, you should know where
 *  the attack came from.
 */
    if (mtmp->mundetected && (hides_under(mdat) || mdat->mlet == S_EEL)) {
        mtmp->mundetected = 0;
        if (!(Blind ? Blind_telepat : Unblind_telepat)) {
            struct obj *obj;
            const char *what;

            if ((obj = level.objects[mtmp->mx][mtmp->my]) != 0) {
                if (Blind && !obj->dknown)
                    what = something;
                else if (is_pool(mtmp->mx, mtmp->my) && !Underwater)
                    what = "the water";
                else
                    what = doname(obj);

                pline("%s was hidden under %s!", Amonnam(mtmp), what);
            }
            newsym(mtmp->mx, mtmp->my);
        }
    }

    /* First determine the base damage done */
    dmg = d((int)mattk->damn, (int)mattk->damd);
    if ((is_undead(mdat) || is_vampshifter(mtmp)) && midnight()) {
        dmg += d((int)mattk->damn, (int)mattk->damd); /* extra damage */
    }

    /* Next a cancellation factor  */
    /* Use uncancelled when the cancellation factor takes into account certain
     * armor's special magic protection.  Otherwise just use !mtmp->mcan.
     */
    armpro = magic_negation(&youmonst);
    uncancelled = !mtmp->mcan && ((rn2(3) >= armpro) || !rn2(50));

    permdmg = 0;
    /* Now, adjust damages via resistances or specific attacks */
    switch (mattk->adtyp) {
    case AD_PHYS:
        if (mattk->aatyp == AT_HUGS && !sticks(youmonst.data)) {
            if (!u.ustuck && rn2(2)) {
                if (u_slip_free(mtmp, mattk)) {
                    dmg = 0;
                } else {
                    u.ustuck = mtmp;
                    pline("%s grabs you!", Monnam(mtmp));
                }
            } else if (u.ustuck == mtmp) {
                exercise(A_STR, FALSE);
                You("are being %s.",
                    (mtmp->data == &mons[PM_ROPE_GOLEM])
                    ? "choked" : "crushed");
            }
        } else {              /* hand to hand weapon */
            struct obj *otmp = mon_currwep;

            if (mattk->aatyp == AT_WEAP && otmp) {
                struct obj *marmg;
                int tmp;

                if (otmp->otyp == CORPSE
                    && touch_petrifies(&mons[otmp->corpsenm])) {
                    dmg = 1;
                    pline("%s hits you with the %s corpse.",
                          Monnam(mtmp), mons[otmp->corpsenm].mname);
                    if (!Stoned)
                        goto do_stone;
                }
                dmg += dmgval(otmp, &youmonst);
                if ((marmg = which_armor(mtmp, W_ARMG)) && marmg->otyp == GAUNTLETS_OF_POWER) {
                    dmg += rn1(4, 3); /* 3..6 */
                }
                if (objects[otmp->otyp].oc_material == SILVER &&
                    hates_silver(youmonst.data)) {
                    pline("The silver sears your flesh!");
                    exercise(A_CON, FALSE);
                }
                if (dmg <= 0) dmg = 1;
                if (!(otmp->oartifact &&
                      artifact_hit(mtmp, &youmonst, otmp, &dmg, dieroll)))
                    hitmsg(mtmp, mattk);
                if (!dmg) break;
                if (u.mh > 1 && u.mh > ((u.uac>0) ? dmg : dmg+u.uac) &&
                    objects[otmp->otyp].oc_material == IRON &&
                    (u.umonnum==PM_BLACK_PUDDING
                     || u.umonnum==PM_BROWN_PUDDING)) {
                    /* This redundancy necessary because you have to
                     * take the damage _before_ being cloned.
                     */
                    if (u.uac < 0) dmg += u.uac;
                    if (dmg < 1) dmg = 1;
                    if (dmg > 1) exercise(A_STR, FALSE);
                    u.mh -= dmg;
                    flags.botl = 1;
                    dmg = 0;
                    if(cloneu())
                        You("divide as %s hits you!", mon_nam(mtmp));
                }
                rustm(&youmonst, otmp);
            } else if (mattk->aatyp != AT_TUCH || dmg != 0 ||
                       mtmp != u.ustuck)
                hitmsg(mtmp, mattk);
        }
        break;

    case AD_DISE:
        hitmsg(mtmp, mattk);
        if (!diseasemu(mdat)) dmg = 0;
        break;

    case AD_FIRE:
        hitmsg(mtmp, mattk);
        if (uncancelled) {
            pline("You're %s!", on_fire(youmonst.data, mattk));
            if (completelyburns(youmonst.data)) {
                You("go up in flames!");
                /* KMH -- this is okay with unchanging */
                rehumanize();
                break;
            } else if (Fire_resistance) {
                pline_The("fire doesn't feel hot!");
                dmg = 0;
            }
            if((int) mtmp->m_lev > rn2(20))
                destroy_item(SCROLL_CLASS, AD_FIRE);
            if((int) mtmp->m_lev > rn2(20))
                destroy_item(POTION_CLASS, AD_FIRE);
            if((int) mtmp->m_lev > rn2(25))
                destroy_item(SPBOOK_CLASS, AD_FIRE);
            burn_away_slime();
        } else dmg = 0;
        break;

    case AD_COLD:
        hitmsg(mtmp, mattk);
        if (uncancelled) {
            pline("You're covered in frost!");
            if (Cold_resistance) {
                pline_The("frost doesn't seem cold!");
                dmg = 0;
            }
            if((int) mtmp->m_lev > rn2(20))
                destroy_item(POTION_CLASS, AD_COLD);
        } else dmg = 0;
        break;

    case AD_ELEC:
        hitmsg(mtmp, mattk);
        if (uncancelled) {
            You("get zapped!");
            if (Shock_resistance) {
                pline_The("zap doesn't shock you!");
                dmg = 0;
            }
            if((int) mtmp->m_lev > rn2(20))
                destroy_item(WAND_CLASS, AD_ELEC);
            if((int) mtmp->m_lev > rn2(20))
                destroy_item(RING_CLASS, AD_ELEC);
        } else dmg = 0;
        break;

    case AD_SLEE:
        hitmsg(mtmp, mattk);
        if (uncancelled && multi >= 0 && !rn2(5)) {
            if (Sleep_resistance) break;
            fall_asleep(-rnd(10), TRUE);
            if (Blind) You("are put to sleep!");
            else You("are put to sleep by %s!", mon_nam(mtmp));
        }
        break;

    case AD_BLND:
        if (can_blnd(mtmp, &youmonst, mattk->aatyp, (struct obj*)0)) {
            if (!Blind) pline("%s blinds you!", Monnam(mtmp));
            make_blinded(Blinded+(long)dmg, FALSE);
            if (!Blind) Your("%s", vision_clears);
        }
        dmg = 0;
        break;

    case AD_DRST:
        ptmp = A_STR;
        goto dopois;

    case AD_DRDX:
        ptmp = A_DEX;
        goto dopois;

    case AD_DRCO:
        ptmp = A_CON;
dopois:
        hitmsg(mtmp, mattk);
        if (uncancelled && !rn2(8)) {
            Sprintf(buf, "%s %s",
                    s_suffix(Monnam(mtmp)), mpoisons_subj(mtmp, mattk));
            poisoned(buf, ptmp, mdat->mname, 30);
        }
        break;

    case AD_DRIN:
        hitmsg(mtmp, mattk);
        if (defends(AD_DRIN, uwep) || !has_head(youmonst.data)) {
            You("don't seem harmed.");
            /* Not clear what to do for green slimes */
            break;
        }
        if (u_slip_free(mtmp, mattk)) break;

        if (uarmh && rn2(8)) {
            /* not body_part(HEAD) */
            Your("%s blocks the attack to your head.", helm_simple_name(uarmh));
            break;
        }
        /* negative armor class doesn't reduce this damage */
        if (Half_physical_damage) dmg = (dmg+1) / 2;
        mdamageu(mtmp, dmg);
        dmg = 0; /* don't inflict a second dose below */

        if (!uarmh || uarmh->otyp != DUNCE_CAP) {
            Your("brain is eaten!");
            /* No such thing as mindless players... */
            if (ABASE(A_INT) <= ATTRMIN(A_INT)) {
                int lifesaved = 0;
                struct obj *wore_amulet = uamul;

                /* Set lives to 0; avoids small loop in heaven or hell modes.
                   Player dies regardless of lives left when dies to
                   brainlessness. */
                u.ulives = 0;

                while(1) {
                    /* avoid looping on "die(y/n)?" */
                    if (lifesaved && (discover || wizard || iflags.debug_fuzzer)) {
                        if (wore_amulet && !uamul) {
                            /* used up AMULET_OF_LIFE_SAVING; still
                               subject to dying from brainlessness */
                            wore_amulet = 0;
                        } else {
                            /* explicitly chose not to die;
                               arbitrarily boost intelligence */
                            ABASE(A_INT) = ATTRMIN(A_INT) + 2;
                            You_feel("like a scarecrow.");
                            break;
                        }
                    }

                    if (lifesaved)
                        pline("Unfortunately your brain is still gone.");
                    else
                        Your("last thought fades away.");
                    Strcpy(killer.name, "brainlessness");
                    killer.format = KILLED_BY;
                    done(DIED);
                    lifesaved++;
                }
            }
        }
        /* adjattrib gives dunce cap message when appropriate */
        (void) adjattrib(A_INT, -rnd(2), FALSE);
        /*  only Cthulhu makes you amnesiac */
        if (mtmp->data == &mons[PM_CTHULHU]) {
            forget_levels(25);  /* lose memory of 25% of levels */
            forget_objects(25); /* lose memory of 25% of objects */
        }
        exercise(A_WIS, FALSE);
        break;

    case AD_PLYS:
        hitmsg(mtmp, mattk);
        if (uncancelled && multi >= 0 && !rn2(3)) {
            if (Free_action) {
                You("momentarily stiffen.");
            } else {
                if (Blind) You("are frozen!");
                else You("are frozen by %s!", mon_nam(mtmp));
                nomovemsg = 0; /* default: "you can move again" */
                nomul(-rnd(10), "paralyzed by a monster");
                exercise(A_DEX, FALSE);
            }
        }
        break;

    case AD_DRLI:
        hitmsg(mtmp, mattk);
        /* if vampire biting (and also a pet) */
        if (is_vampire(mtmp->data) && mattk->aatyp == AT_BITE &&
            has_blood(youmonst.data)) {
            Your("blood is being drained!");
            /* Get 1/20th of full corpse value
             * Therefore 4 bites == 1 drink
             */
            if (mtmp->mtame && !mtmp->isminion)
                EDOG(mtmp)->hungrytime += ((int)((youmonst.data)->cnutrit / 20) + 1);
        }

        if (uncancelled && !rn2(3) && !Drain_resistance) {
            losexp("life drainage");
        }
        break;

    case AD_LEGS:
    { long side = rn2(2) ? RIGHT_SIDE : LEFT_SIDE;
      const char *sidestr = (side == RIGHT_SIDE) ? "right" : "left";

      /* This case is too obvious to ignore, but Nethack is not in
       * general very good at considering height--most short monsters
       * still _can_ attack you when you're flying or mounted.
       */
      if ((u.usteed || Levitation || Flying) && !is_flyer(mtmp->data)) {
          pline("%s tries to reach your %s %s!", Monnam(mtmp),
                sidestr, body_part(LEG));
          dmg = 0;
      } else if (mtmp->mcan) {
          pline("%s nuzzles against your %s %s!", Monnam(mtmp),
                sidestr, body_part(LEG));
          dmg = 0;
      } else {
          if (uarmf) {
              if (rn2(2) && (uarmf->otyp == LOW_BOOTS ||
                             uarmf->otyp == IRON_SHOES))
                  pline("%s pricks the exposed part of your %s %s!",
                        Monnam(mtmp), sidestr, body_part(LEG));
              else if (!rn2(5))
                  pline("%s pricks through your %s boot!",
                        Monnam(mtmp), sidestr);
              else {
                  pline("%s scratches your %s boot!", Monnam(mtmp),
                        sidestr);
                  dmg = 0;
                  break;
              }
          } else pline("%s pricks your %s %s!", Monnam(mtmp),
                       sidestr, body_part(LEG));
          set_wounded_legs(side, rnd(60-ACURR(A_DEX)));
          exercise(A_STR, FALSE);
          exercise(A_DEX, FALSE);
      }
      break;}

    case AD_HEAD:
        if ((!rn2(40) || youmonst.data->mlet == S_JABBERWOCK) && !mtmp->mcan) {
            if (!has_head(youmonst.data)) {
                pline("Somehow, %s misses you wildly.", mon_nam(mtmp));
                dmg = 0;
                break;
            }
            if (noncorporeal(youmonst.data) || amorphous(youmonst.data)) {
                pline("%s slices through your %s.",
                      Monnam(mtmp), body_part(NECK));
                break;
            }
            pline("%s %ss you!", Monnam(mtmp),
                  rn2(2) ? "behead" : "decapitate");
            if (Upolyd) rehumanize();
            else done_in_by(mtmp);
            dmg = 0;
        }
        else hitmsg(mtmp, mattk);
        break;

    case AD_STON: /* cockatrice */
        hitmsg(mtmp, mattk);
        if (!rn2(3)) {
            if (mtmp->mcan) {
                if (!Deaf) {
                    You_hear("a cough from %s!", mon_nam(mtmp));
                }
            } else {
                if (!Deaf) {
                    You_hear("%s hissing!", s_suffix(mon_nam(mtmp)));
                }
                if(!rn2(10) ||
                   (flags.moonphase == NEW_MOON && !have_lizard())) {
do_stone:
                    if (!Stoned && !Stone_resistance
                        && !(poly_when_stoned(youmonst.data) &&
                             polymon(PM_STONE_GOLEM))) {
                        int kformat = KILLED_BY_AN;
                        const char *kname = mtmp->data->mname;

                        if (mtmp->data->geno & G_UNIQ) {
                            if (!type_is_pname(mtmp->data)) {
                                kname = the(kname);
                            }
                            kformat = KILLED_BY;
                        }
                        make_stoned(5L, (char *) 0, kformat, kname);
                        return(1);
                        /* done_in_by(mtmp); */
                    }
                }
            }
        }
        break;

    case AD_STCK:
        hitmsg(mtmp, mattk);
        if (uncancelled && !u.ustuck && !sticks(youmonst.data))
            u.ustuck = mtmp;
        break;

    case AD_WRAP:
        if ((!mtmp->mcan || u.ustuck == mtmp) && !sticks(youmonst.data)) {
            /* vegetation never misses */
            if (!u.ustuck && (!rn2(10) || is_vegetation(mdat))) {
                if (u_slip_free(mtmp, mattk)) {
                    dmg = 0;
                } else {
                    urgent_pline("%s %s itself around you!", Monnam(mtmp),
                          is_vegetation(mdat) ? "winds" : "swings");
                    u.ustuck = mtmp;
                }
            } else if(u.ustuck == mtmp) {
                if (is_pool(mtmp->mx, mtmp->my) && !Swimming
                    && !Amphibious) {
                    boolean moat =
                        (levl[mtmp->mx][mtmp->my].typ != POOL) &&
                        (levl[mtmp->mx][mtmp->my].typ != WATER) &&
                        !Is_medusa_level(&u.uz) &&
                        !Is_waterlevel(&u.uz);

                    urgent_pline("%s drowns you...", Monnam(mtmp));
                    killer.format = KILLED_BY_AN;
                    Sprintf(killer.name, "%s by %s",
                            moat ? "moat" : "pool of water",
                            an(mtmp->data->mname));
                    done(DROWNING);
                } else if(mattk->aatyp == AT_HUGS)
                    You("are being crushed.");
            } else {
                dmg = 0;
                if(flags.verbose)
                    pline("%s brushes against your %s.", Monnam(mtmp),
                          body_part(LEG));
            }
        } else dmg = 0;
        break;

    case AD_WERE:
        hitmsg(mtmp, mattk);
        if (uncancelled && !rn2(4) && u.ulycn == NON_PM &&
            !Protection_from_shape_changers &&
            !defends(AD_WERE, uwep)) {
            urgent_pline("You feel feverish.");
            exercise(A_CON, FALSE);
            set_ulycn(monsndx(mdat));
            retouch_equipment(2);
        }
        break;

    case AD_SGLD:
        hitmsg(mtmp, mattk);
        if (youmonst.data->mlet == mdat->mlet) break;
        if(!mtmp->mcan) stealgold(mtmp);
        break;

    case AD_SITM:       /* for now these are the same */
    case AD_SEDU: {
        int is_robber = (is_animal(mtmp->data) ||
                         mtmp->data->mlet == S_HUMAN);
        if (is_robber) {
            hitmsg(mtmp, mattk);
            if (mtmp->mcan) break;
            /* Continue below */
        } else if (dmgtype(youmonst.data, AD_SEDU)
#ifdef SEDUCE
                   || dmgtype(youmonst.data, AD_SSEX)
#endif
                   ) {
            pline("%s %s.", Monnam(mtmp), mtmp->minvent ?
                  "brags about the goods some dungeon explorer provided" :
                  "makes some remarks about how difficult theft is lately");
            if (!tele_restrict(mtmp)) (void) rloc(mtmp, FALSE);
            return 3;
        } else if (mtmp->mcan) {
            if (!Blind) {
                pline("%s tries to %s you, but you seem %s.",
                      Adjmonnam(mtmp, "plain"),
                      flags.female ? "charm" : "seduce",
                      flags.female ? "unaffected" : "uninterested");
            }
            if(rn2(3)) {
                if (!tele_restrict(mtmp)) (void) rloc(mtmp, FALSE);
                return 3;
            }
            break;
        }
        buf[0] = '\0';
        switch (steal(mtmp, buf)) {
        case -1:
            return 2;
        case 0:
            break;
        default:
            if (!is_robber && !tele_restrict(mtmp))
                (void) rloc(mtmp, FALSE);
            if (is_robber && *buf) {
                if (canseemon(mtmp))
                    pline("%s tries to %s away with %s.",
                          Monnam(mtmp),
                          locomotion(mtmp->data, "run"),
                          buf);
            }
            monflee(mtmp, 0, FALSE, FALSE);
            return 3;
        }
        break;
    }
#ifdef SEDUCE
    case AD_SSEX:
        if(could_seduce(mtmp, &youmonst, mattk) == 1
           && !mtmp->mcan)
            if (doseduce(mtmp))
                return 3;
        break;
#endif
    case AD_SAMU:
        hitmsg(mtmp, mattk);
        /* when the Wiz hits, 1/20 steals the amulet */
        if (!rn2(20)) stealamulet(mtmp);
        break;

    case AD_TLPT:
        hitmsg(mtmp, mattk);
        if (uncancelled) {
            if(flags.verbose)
                Your("position suddenly seems very uncertain!");
            tele();
            /* As of 3.6.2:  make sure damage isn't fatal; previously, it
               was possible to be teleported and then drop dead at
               the destination when QM's 1d4 damage gets applied below;
               even though that wasn't "wrong", it seemed strange,
               particularly if the teleportation had been controlled
               [applying the damage first and not teleporting if fatal
               is another alternative but it has its own complications] */
            int tmphp;
            if ((Half_physical_damage ? (dmg - 1) / 2 : dmg) >= (tmphp = (Upolyd ? u.mh : u.uhp))) {
                dmg = tmphp - 1;
                if (Half_physical_damage) {
                    dmg *= 2; /* doesn't actually increase damage; we only
                               * get here if half the original damage would
                               * would have been fatal, so double reduced
                               * damage will be less than original damage */
                }
                if (dmg < 1) {
                    /* implies (tmphp <= 1) */
                    dmg = 1;
                    /* this might increase current HP beyond maximum HP but
                       it will be immediately reduced below, so that should
                       be indistinguishable from zero damage; we don't drop
                       damage all the way to zero because that inhibits any
                       passive counterattack if poly'd hero has one */
                    if (Upolyd && u.mh == 1) {
                        u.mh++;
                    } else if (!Upolyd && u.uhp == 1) {
                        u.uhp++;
                    }
                    /* [don't set context.botl here] */
                }
            }
        }
        break;

    case AD_LVLT:
        hitmsg(mtmp, mattk);
        if (uncancelled) {
            if(flags.verbose) {
                if (Teleport_control) {
                    You("feel like you could have lost some potential.");
                } else {
                    You("suddenly feel like you've lost some potential.");
                }
            }
            level_tele();
        }
        break;

    case AD_RUST:
        hitmsg(mtmp, mattk);
        if (mtmp->mcan) break;
        if (u.umonnum == PM_IRON_GOLEM) {
            You("rust!");
            /* KMH -- this is okay with unchanging */
            rehumanize();
            break;
        }
        erode_armor(&youmonst, ERODE_RUST);
        break;

    case AD_CORR:
        hitmsg(mtmp, mattk);
        if (mtmp->mcan) break;
        erode_armor(&youmonst, ERODE_CORRODE);
        break;

    case AD_DCAY:
        hitmsg(mtmp, mattk);
        if (mtmp->mcan) break;
        if (u.umonnum == PM_WOOD_GOLEM ||
            u.umonnum == PM_LEATHER_GOLEM) {
            You("rot!");
            /* KMH -- this is okay with unchanging */
            rehumanize();
            break;
        }
        erode_armor(&youmonst, ERODE_ROT);
        break;

    case AD_HEAL:
        /* a cancelled nurse is just an ordinary monster
         * nurses don't heal those that cause petrification */
        if (mtmp->mcan || (Upolyd && touch_petrifies(youmonst.data))) {
            hitmsg(mtmp, mattk);
            break;
        }
        /* this condition must match the one in sounds.c for MS_NURSE */
        if ((!(uwep && (uwep->oclass == WEAPON_CLASS || is_weptool(uwep))))
            && !uarmu
            && !uarm && !uarmh && !uarms && !uarmg && !uarmc && !uarmf) {
            boolean goaway = FALSE;
            pline("%s hits!  (I hope you don't mind.)", Monnam(mtmp));
            if (Upolyd) {
                u.mh += rnd(7);
                if (!rn2(7)) {
                    /* no upper limit necessary; effect is temporary */
                    u.mhmax++;
                    if (!rn2(13)) goaway = TRUE;
                }
                if (u.mh > u.mhmax) u.mh = u.mhmax;
            } else {
                u.uhp += rnd(7);
                if (!rn2(7)) {
                    /* hard upper limit via nurse care: 25 * ulevel */
                    if (u.uhpmax < 5 * u.ulevel + d(2 * u.ulevel, 10))
                        u.uhpmax++;
                    if (!rn2(13)) goaway = TRUE;
                }
                if (u.uhp > u.uhpmax) u.uhp = u.uhpmax;
            }
            check_uhpmax();
            if (!rn2(3)) exercise(A_STR, TRUE);
            if (!rn2(3)) exercise(A_CON, TRUE);
            if (Sick) make_sick(0L, (char *) 0, FALSE, SICK_ALL);
            flags.botl = 1;
            if (goaway) {
                mongone(mtmp);
                return 2;
            } else if (!rn2(33)) {
                if (!tele_restrict(mtmp)) (void) rloc(mtmp, FALSE);
                monflee(mtmp, d(3, 6), TRUE, FALSE);
                return 3;
            }
            dmg = 0;
        } else {
            if (Role_if(PM_HEALER)) {
                if (!Deaf && !(moves % 5)) {
                    verbalize("Doc, I can't help you unless you cooperate.");
                }
                dmg = 0;
            } else hitmsg(mtmp, mattk);
        }
        break;

    case AD_CURS:
        hitmsg(mtmp, mattk);
        if(!night() && mdat == &mons[PM_GREMLIN]) break;
        if(!mtmp->mcan && !rn2(10)) {
            if (!Deaf) {
                if (Blind) You_hear("laughter.");
                else pline("%s chuckles.", Monnam(mtmp));
            }
            if (u.umonnum == PM_CLAY_GOLEM) {
                pline("Some writing vanishes from your head!");
                /* KMH -- this is okay with unchanging */
                rehumanize();
                break;
            }
            attrcurse();
        }
        break;

    case AD_STUN:
        hitmsg(mtmp, mattk);
        if (!mtmp->mcan && !rn2(4)) {
            make_stunned(HStun + dmg, TRUE);
            dmg /= 2;
        }
        break;

    case AD_ACID:
        hitmsg(mtmp, mattk);
        if (!mtmp->mcan && !rn2(3))
            if (Acid_resistance) {
                pline("You're covered in %s, but it seems harmless.", hliquid("acid"));
                dmg = 0;
            } else {
                pline("You're covered in %s!  It burns!", hliquid("acid"));
                exercise(A_STR, FALSE);
            }
        else dmg = 0;
        break;

    case AD_SLOW:
        hitmsg(mtmp, mattk);
        if (uncancelled && HFast &&
            !defends(AD_SLOW, uwep) && !rn2(4))
            u_slow_down();
        break;

    case AD_DREN:
        hitmsg(mtmp, mattk);
        if (uncancelled && !rn2(4)) { /* 25% chance */
            drain_en(dmg);
        }
        dmg = 0;
        break;

    case AD_CONF:
        hitmsg(mtmp, mattk);
        if (!mtmp->mcan && !rn2(4) && !mtmp->mspec_used) {
            mtmp->mspec_used = mtmp->mspec_used + (dmg + rn2(6));
            if(Confusion)
                You("are getting even more confused.");
            else You("are getting confused.");
            make_confused(HConfusion + dmg, FALSE);
        }
        dmg = 0;
        break;

    case AD_DETH:
        pline("%s reaches out with its deadly touch.", Monnam(mtmp));
        if (is_undead(youmonst.data)) {
            /* Still does normal damage */
            pline("Was that the touch of death?");
            break;
        }
        switch (rn2(20)) {
        case 19:
        case 18:
        case 17:
            if (!Antimagic) {
                killer.format = KILLED_BY_AN;
                Strcpy(killer.name, "touch of death");
                done(DIED);
                dmg = 0;
                break;
            }
            /* fall through */
        default: /* case 16: ... case 5: */
            You_feel("your life force draining away...");
            permdmg = 1;    /* actual damage done below */
            break;
        case 4: case 3: case 2: case 1: case 0:
            if (Antimagic) shieldeff(u.ux, u.uy);
            pline("Lucky for you, it didn't work!");
            dmg = 0;
            break;
        }
        break;

    case AD_PEST:
        pline("%s reaches out, and you feel fever and chills.",
              Monnam(mtmp));
        (void) diseasemu(mdat); /* plus the normal damage */
        break;

    case AD_FAMN:
        pline("%s reaches out, and your body shrivels.",
              Monnam(mtmp));
        exercise(A_CON, FALSE);
        if (!is_fainted()) morehungry(rn1(40, 40));
        /* plus the normal damage */
        break;

    case AD_SLIM:
        hitmsg(mtmp, mattk);
        if (!uncancelled) break;
        if (flaming(youmonst.data)) {
            pline_The("slime burns away!");
            dmg = 0;
        } else if (Unchanging || noncorporeal(youmonst.data) ||
                   youmonst.data == &mons[PM_GREEN_SLIME]) {
            You("are unaffected.");
            dmg = 0;
        } else if (!Slimed) {
            You("don't feel very well.");
            make_slimed(10L, (char *) 0);
            delayed_killer(SLIMED, KILLED_BY_AN, mtmp->data->mname);
        } else
            pline("Yuck!");
        break;

    case AD_FREZ:
        /* don't check for uncancelled. Freezing attack
         * doesn't directly attack the player character itself
         * so it's justified. */
        hitmsg(mtmp, mattk);
        maybe_freeze_u(&dmg);
        break;

    case AD_ENCH: /* KMH -- remove enchantment (disenchanter) */
        hitmsg(mtmp, mattk);
        /* uncancelled is sufficient enough; please
           don't make this attack less frequent */
        if (uncancelled) {
            struct obj *obj = some_armor(&youmonst);

            if (!obj) {
                /* some rings are susceptible;
                   amulets and blindfolds aren't (at present) */
                switch (rn2(5)) {
                case 0:
                    break;
                case 1:
                    obj = uright;
                    break;
                case 2:
                    obj = uleft;
                    break;
                case 3:
                    obj = uamul;
                    break;
                case 4:
                    obj = ublindf;
                    break;
                }
            }
            if (drain_item(obj, FALSE)) {
                Your("%s less effective.", aobjnam(obj, "seem"));
            }

        }
        break;

#ifdef WEBB_DISINT
    case AD_DISN:
        hitmsg(mtmp, mattk);
        if (!mtmp->mcan && mtmp->mhp>6) {
            int mass = 0, touched = 0;
            struct obj * destroyme = 0;
            if (Disint_resistance) {
                break;
            }
            if (uarms) {
                if (!oresist_disintegration(uarms))
                    destroyme = uarms;
            } else {
                switch (rn2(10)) { /* where it hits you */
                case 0:     /* head */
                case 1:
                    if (uarmc && (uarmc->otyp == DWARVISH_CLOAK ||
                                  uarmc->otyp == MUMMY_WRAPPING)) {
                        if (!oresist_disintegration(uarmc)) {
                            destroyme = uarmc;
                        }
                    } else if (uarmh) {
                        if (!oresist_disintegration(uarmh)) {
                            destroyme = uarmh;
                        }
                    } else
                        touched = 1;
                    break;
                case 2:     /* feet */
                    if (uarmf) {
                        if (!oresist_disintegration(uarmf))
                            destroyme = uarmf;
                    } else
                        touched = 1;
                    break;
                case 3:     /* hands (right) */
                case 4:
                    if (uwep) {
                        if (!oresist_disintegration(uwep)) {
                            struct obj * otmp = uwep;
                            mass = otmp->owt;
                            u.twoweap = FALSE;
                            uwepgone();
                            useup(otmp);
                            dmg = 0;
                        }
                    } else if (uarmg) {
                        if (!oresist_disintegration(uarmg))
                            destroyme = uarmg;
                    } else
                        touched = 1;
                    break;
                default:     /* main body hit */
                    if (uarmc) {
                        if (!oresist_disintegration(uarmc))
                            destroyme = uarmc;
                    } else if (uarm) {
                        if (!oresist_disintegration(uarm))
                            destroyme = uarm;
                    } else if (uarmu) {
                        if (!oresist_disintegration(uarmu))
                            destroyme = uarmu;
                    } else
                        touched = 1;
                    break;
                }
            }
            if (destroyme) {
                mass = destroyme->owt;
                destroy_arm(destroyme);
                dmg = 0;
            } else if (touched) {
                int recip_damage = instadisintegrate(an(mtmp->data->mname));
                if (recip_damage) {
                    dmg=0;
                    mtmp->mhp -= recip_damage;
                }
            }
            if (mass) {
                weight_dmg(mass);
                mtmp->mhp -= mass;
            }
        }
        break;
#endif
    default:
        dmg = 0;
        break;
    }
    if ((Upolyd ? u.mh : u.uhp) < 1) {
        /* already dead? call rehumanize() or done_in_by() as appropriate */
        mdamageu(mtmp, 1);
        dmg = 0;
    }

/*  Negative armor class reduces damage done instead of fully protecting
 *  against hits.
 */
    if (dmg && u.uac < 0) {
        dmg -= rnd(-u.uac);
        if (dmg < 1) dmg = 1;
    }

    if(dmg) {
        if (Half_physical_damage
            /* Mitre of Holiness */
            || (Role_if(PM_PRIEST) && uarmh && is_quest_artifact(uarmh) &&
                (is_undead(mtmp->data) || is_demon(mtmp->data) || is_vampshifter(mtmp)))) {
            dmg = (dmg+1) / 2;
        }

        if (permdmg) {  /* Death's life force drain */
            int lowerlimit, *hpmax_p;
            /*
             * Apply some of the damage to permanent hit points:
             *  polymorphed         100% against poly'd hpmax
             *  hpmax > 25*lvl      100% against normal hpmax
             *  hpmax > 10*lvl  50..100%
             *  hpmax >  5*lvl  25..75%
             *  otherwise        0..50%
             * Never reduces hpmax below 1 hit point per level.
             */
            permdmg = rn2(dmg / 2 + 1);
            if (Upolyd || u.uhpmax > 25 * u.ulevel) permdmg = dmg;
            else if (u.uhpmax > 10 * u.ulevel) permdmg += dmg / 2;
            else if (u.uhpmax > 5 * u.ulevel) permdmg += dmg / 4;

            if (Upolyd) {
                hpmax_p = &u.mhmax;
                /* [can't use youmonst.m_lev] */
                lowerlimit = min((int)youmonst.data->mlevel, u.ulevel);
            } else {
                hpmax_p = &u.uhpmax;
                lowerlimit = u.ulevel;
            }
            if (*hpmax_p - permdmg > lowerlimit) {
                *hpmax_p -= permdmg;
            } else if (*hpmax_p > lowerlimit) {
                *hpmax_p = lowerlimit;
            } else {
                /* unlikely... */
                /* already at or below minimum threshold; do nothing */
            }
            flags.botl = 1;
        }

        mdamageu(mtmp, dmg);
    }

    if (dmg)
        res = passiveum(olduasmon, mtmp, mattk);
    else
        res = 1;
    stop_occupation();
    return res;
}

/* An interface for use when taking a blindfold off, for example,
 * to see if an engulfing attack should immediately take affect, like
 * a passive attack. TRUE if engulfing blindness occurred */
boolean
gulp_blnd_check(void)
{
    struct attack *mattk;

    if (!Blinded &&
         u.uswallow &&
         (mattk = attacktype_fordmg(u.ustuck->data, AT_ENGL, AD_BLND)) &&
         can_blnd(u.ustuck, &youmonst, mattk->aatyp, (struct obj *) 0)) {
        ++u.uswldtim; /* compensate for gulpmu change */
        (void) gulpmu(u.ustuck, mattk);
        return TRUE;
    }
    return FALSE;
}

/* monster swallows you, or damage if u.uswallow */
static int
gulpmu(struct monst *mtmp, struct attack *mattk)
{
    struct trap *t = t_at(u.ux, u.uy);
    int tmp = d((int)mattk->damn, (int)mattk->damd);
    int tim_tmp;
    struct obj *otmp2;
    int i;
    boolean physical_damage = FALSE;

    if (!u.uswallow) {  /* swallows you */
        int omx = mtmp->mx, omy = mtmp->my;

        if (!engulf_target(mtmp, &youmonst)) {
            return 0;
        }
        if ((t && ((t->ttyp == PIT) || (t->ttyp == SPIKED_PIT))) &&
            sobj_at(BOULDER, u.ux, u.uy))
            return(0);

        if (Punished) unplacebc();  /* ball&chain go away */
        remove_monster(mtmp->mx, mtmp->my);
        mtmp->mtrapped = 0;     /* no longer on old trap */
        place_monster(mtmp, u.ux, u.uy);
        u.ustuck = mtmp;
        newsym(mtmp->mx, mtmp->my);
        if (is_animal(mtmp->data) && u.usteed) {
            char buf[BUFSZ];

            /* Too many quirks presently if hero and steed
             * are swallowed. Pretend purple worms don't
             * like horses for now :-)
             */
            Strcpy(buf, mon_nam(u.usteed));
            urgent_pline ("%s lunges forward and plucks you off %s!",
                   Monnam(mtmp), buf);
            dismount_steed(DISMOUNT_ENGULFED);
        } else {
            urgent_pline("%s engulfs you!", Monnam(mtmp));
        }
        stop_occupation();
        reset_occupations();    /* behave as if you had moved */

        if (u.utrap) {
            You("are released from the %s!",
                u.utraptype==TT_WEB ? "web" : "trap");
            u.utrap = 0;
        }
        if (u.ufeetfrozen) {
            You("break free from the ice!");
            u.ufeetfrozen = 0;
        }

        i = number_leashed();
        if (i > 0) {
            const char *s = (i > 1) ? "leashes" : "leash";

            pline_The("%s %s loose.", s, vtense(s, "snap"));
            unleash_all();
        }

        if (touch_petrifies(youmonst.data) && !resists_ston(mtmp)) {
            /* put the attacker back where it started;
               the resulting statue will end up there */
            remove_monster(mtmp->mx, mtmp->my); /* u.ux,u.uy */
            place_monster(mtmp, omx, omy);
            minstapetrify(mtmp, TRUE);
            /* normally unstuck() would do this, but we're not
               fully swallowed yet so that won't work here */
            if (Punished) {
                placebc();
            }
            u.ustuck = 0;
            return (!DEADMONSTER(mtmp)) ? 0 : 2;
        }

        display_nhwindow(WIN_MESSAGE, FALSE);
        vision_recalc(2);   /* hero can't see anything */
        u.uswallow = 1;
        /* for digestion, shorter time is more dangerous;
           for other swallowings, longer time means more
           chances for the swallower to attack */
        if (mattk->adtyp == AD_DGST) {
            tim_tmp = 25 - (int) mtmp->m_lev;
            if (tim_tmp > 0) {
                tim_tmp = rnd(tim_tmp) / 2;
            } else if (tim_tmp < 0) {
                tim_tmp = -(rnd(-tim_tmp) / 2);
            }
            /* having good armor & high constitution makes
               it take longer for you to be digested, but
               you'll end up trapped inside for longer too */
            tim_tmp += -u.uac + 10 + (ACURR(A_CON) / 3 - 1);
        } else {
            /* higher level attacker takes longer to eject hero */
            tim_tmp = rnd((int) mtmp->m_lev + 10 / 2);
        }
        /* u.uswldtim always set > 1 */
        u.uswldtim = (unsigned)((tim_tmp < 2) ? 2 : tim_tmp);
        swallowed(1);
        for (otmp2 = invent; otmp2; otmp2 = otmp2->nobj)
            (void) snuff_lit(otmp2);
    }

    if (mtmp != u.ustuck) {
        return 0;
    }
    if (Punished) {
        /* ball&chain are in limbo while swallowed; update their internal
           location to be at swallower's spot */
        if (uchain->where == OBJ_FREE) {
            uchain->ox = mtmp->mx, uchain->oy = mtmp->my;
        }
        if (uball->where == OBJ_FREE) {
            uball->ox = mtmp->mx, uball->oy = mtmp->my;
        }
    }
    if (u.uswldtim > 0) {
        u.uswldtim -= 1;
    }

    switch (mattk->adtyp) {
    case AD_DGST:
        physical_damage = TRUE;
        if (Slow_digestion) {
            /* Messages are handled below */
            u.uswldtim = 0;
            tmp = 0;
        } else if (u.uswldtim == 0) {
            pline("%s totally digests you!", Monnam(mtmp));
            tmp = u.uhp;
            if (Half_physical_damage) tmp *= 2; /* sorry */
        } else {
            pline("%s%s digests you!", Monnam(mtmp),
                  (u.uswldtim == 2) ? " thoroughly" :
                  (u.uswldtim == 1) ? " utterly" : "");
            exercise(A_STR, FALSE);
        }
        break;

    case AD_PHYS:
        physical_damage = TRUE;
        if (mtmp->data == &mons[PM_FOG_CLOUD]) {
            You("are laden with moisture and %s",
                flaming(youmonst.data) ? "are smoldering out!" :
                Breathless ? "find it mildly uncomfortable." :
                amphibious(youmonst.data) ? "feel comforted." :
                "can barely breathe!");
            /* NB: Amphibious includes Breathless */
            if (Amphibious && !flaming(youmonst.data)) tmp = 0;
        } else {
            You("are pummeled with debris!");
            exercise(A_STR, FALSE);
        }
        break;

    case AD_ACID:
        if (Acid_resistance) {
            You("are covered with a seemingly harmless goo.");
            tmp = 0;
        } else {
            if (Hallucination) pline("Ouch!  You've been slimed!");
            else You("are covered in slime!  It burns!");
            exercise(A_STR, FALSE);
        }
        break;

    case AD_BLND:
        if (can_blnd(mtmp, &youmonst, mattk->aatyp, (struct obj*)0)) {
            if (!Blind) {
                long was_blinded = Blinded;
                if (!Blinded) {
                    You_cant("see in here!");
                }
                make_blinded((long)tmp, FALSE);
                if (!was_blinded && !Blind) {
                    Your("%s", vision_clears);
                }
            } else
                /* keep him blind until disgorged */
                make_blinded(Blinded+1, FALSE);
        }
        tmp = 0;
        break;

    case AD_ELEC:
        if (!mtmp->mcan && rn2(2)) {
            pline_The("air around you crackles with electricity.");
            if (Shock_resistance) {
                shieldeff(u.ux, u.uy);
                You("seem unhurt.");
                ugolemeffects(AD_ELEC, tmp);
                tmp = 0;
            }
        } else tmp = 0;
        break;

    case AD_COLD:
        if (!mtmp->mcan && rn2(2)) {
            if (Cold_resistance) {
                shieldeff(u.ux, u.uy);
                You_feel("mildly chilly.");
                ugolemeffects(AD_COLD, tmp);
                tmp = 0;
            } else You("are freezing to death!");
        } else tmp = 0;
        break;

    case AD_FIRE:
        if (!mtmp->mcan && rn2(2)) {
            if (Fire_resistance) {
                shieldeff(u.ux, u.uy);
                You_feel("mildly hot.");
                ugolemeffects(AD_FIRE, tmp);
                tmp = 0;
            } else You("are burning to a crisp!");
            burn_away_slime();
        } else tmp = 0;
        break;

    case AD_DISE:
        if (!diseasemu(mtmp->data)) tmp = 0;
        break;

    case AD_DREN:
        /* AC magic cancellation doesn't help when engulfed */
        if (!mtmp->mcan && rn2(4)) { /* 75% chance */
            drain_en(tmp);
        }
        break;

    case AD_DISN:
        if (!mtmp->mcan && rn2(2) && u.uswldtim < 2) {
            tmp = 0;
            if (Disint_resistance) {
                shieldeff(u.ux, u.uy);
                You_feel("mildly tickled.");
                tmp = 0;
                break;
            } else if (uarms) {
                /* destroy shield; other possessions are safe */
                (void) destroy_arm(uarms);
                break;
            } else if (uarm) {
                /* destroy suit; if present, cloak goes too */
                if (uarmc) (void) destroy_arm(uarmc);
                (void) destroy_arm(uarm);
                break;
            }
            /* no shield or suit, you're dead; wipe out cloak
               and/or shirt in case of life-saving or bones */
            if (uarmc) (void) destroy_arm(uarmc);
            if (uarmu) (void) destroy_arm(uarmu);

            You("are disintegrated!");
            tmp = u.uhp;
            if (Half_physical_damage) tmp *= 2; /* sorry */
        } else {
            tmp = 0;
        }
        break;
    default:
        tmp = 0;
        break;
    }

    if (physical_damage) {
        tmp = Maybe_Half_Phys(tmp);
    }

    mdamageu(mtmp, tmp);
    if (tmp) stop_occupation();

    if (!u.uswallow) {
        ; /* life-saving has already expelled swallowed hero */
    } else if (touch_petrifies(youmonst.data) && !resists_ston(mtmp)) {
        pline("%s very hurriedly %s you!", Monnam(mtmp),
              is_animal(mtmp->data) ? "regurgitates" : "expels");
        expels(mtmp, mtmp->data, FALSE);
    } else if (!u.uswldtim || youmonst.data->msize >= MZ_HUGE) {
        /* As of 3.6.2: u.uswldtim used to be set to 0 by life-saving but it
           expels now so the !u.uswldtim case is no longer possible;
           however, polymorphing into a huge form while already
           swallowed is still possible */
        You("get %s!", is_animal(mtmp->data) ? "regurgitated" : "expelled");
        if (flags.verbose && (is_animal(mtmp->data) ||
                              (dmgtype(mtmp->data, AD_DGST) && Slow_digestion)))
            pline("Obviously %s doesn't like your taste.", mon_nam(mtmp));
        expels(mtmp, mtmp->data, FALSE);
    }
    return(1);
}

/* monster explodes in your face */
static int
explmu(struct monst *mtmp, struct attack *mattk, boolean ufound)
{
    boolean physical_damage = TRUE, kill_agr = TRUE;

    if (mtmp->mcan) return(0);

    if (!ufound)
        pline("%s explodes at a spot in %s!",
              canseemon(mtmp) ? Monnam(mtmp) : "It",
              levl[mtmp->mux][mtmp->muy].typ == WATER
              ? "empty water" : "thin air");
    else {
        int tmp = d((int)mattk->damn, (int)mattk->damd);
        boolean not_affected = defends((int)mattk->adtyp, uwep);

        hitmsg(mtmp, mattk);

        switch (mattk->adtyp) {
        case AD_COLD:
            physical_damage = FALSE;
            not_affected |= Cold_resistance;
            goto common;

        case AD_FIRE:
            physical_damage = FALSE;
            not_affected |= Fire_resistance;
            goto common;

        case AD_ELEC:
            physical_damage = FALSE;
            not_affected |= Shock_resistance;
            goto common;

        case AD_PHYS:
            /* there aren't any exploding creatures with AT_EXPL attack
               for AD_PHYS damage but there might be someday; without this,
               static analysis complains that 'physical_damage' is always
               False when tested below; it's right, but having that in
               place means one less thing to update if AD_PHYS gets added */
common:

            if (!not_affected) {
                if (ACURR(A_DEX) > rnd(20)) {
                    You("duck some of the blast.");
                    tmp = (tmp+1) / 2;
                } else {
                    if (flags.verbose) You("get blasted!");
                }
                if (mattk->adtyp == AD_FIRE) burn_away_slime();
                if (physical_damage) {
                    tmp = Maybe_Half_Phys(tmp);
                }
                mdamageu(mtmp, tmp);
            }
            break;

        case AD_BLND:
            not_affected = resists_blnd(&youmonst);
            if (!not_affected) {
                /* sometimes you're affected even if it's invisible */
                if (mon_visible(mtmp) || (rnd(tmp /= 2) > u.ulevel)) {
                    You("are blinded by a blast of light!");
                    make_blinded((long)tmp, FALSE);
                    if (!Blind) Your("%s", vision_clears);
                } else if (flags.verbose)
                    You("get the impression it was not terribly bright.");
            }
            break;

        case AD_HALU:
            not_affected |= Blind ||
                            (u.umonnum == PM_BLACK_LIGHT ||
                             u.umonnum == PM_VIOLET_FUNGUS ||
                             dmgtype(youmonst.data, AD_STUN));
            if (!not_affected) {
                boolean chg;
                if (!Hallucination)
                    You("are caught in a blast of kaleidoscopic light!");
                /* avoid hallucinating the black light as it dies */
                mondead(mtmp);    /* remove it from map now */
                kill_agr = FALSE; /* already killed (maybe lifesaved) */
                chg = make_hallucinated(HHallucination + (long)tmp, FALSE, 0L);
                You("%s.", chg ? "are freaked out" : "seem unaffected");
            }
            break;

        default:
            break;
        }
        if (not_affected) {
            You("seem unaffected by it.");
            ugolemeffects((int)mattk->adtyp, tmp);
        }
    }
    if (kill_agr) {
        mondead(mtmp);
    }
    wake_nearto(mtmp->mx, mtmp->my, 7*7);
    return (!DEADMONSTER(mtmp)) ? 0 : 2;
}

/* monster gazes at you */
int
gazemu(struct monst *mtmp, struct attack *mattk)
{
    static const char *const reactions[] = {
        "confused",              /* [0] */
        "stunned",               /* [1] */
        "puzzled",   "dazzled",  /* [2,3] */
        "irritated", "inflamed", /* [4,5] */
        "tired",                 /* [6] */
        "dulled",                /* [7] */
    };
    int react = -1;
    boolean cancelled = (mtmp->mcan != 0), already = FALSE;

    /* assumes that hero has to see monster's gaze in order to be
       affected, rather than monster just having to look at hero;
       when hallucinating, hero's brain doesn't what
       it's seeing correctly so the gaze is usually ineffective
       [this could be taken a lot farther and select a gaze effect
       appropriate to what's currently being displayed, giving
       ordinary monsters a gaze attack when hero thinks he or she
       is facing a gazing creature, but let's not go that far...] */
    if (Hallucination && rnf(3,4)) {
        cancelled = TRUE;
    }

    switch (mattk->adtyp) {
    case AD_STON:
        if (mtmp->mcan || !mtmp->mcansee || Hallucination) {
            if (!canseemon(mtmp)) break;    /* silently */
            pline("%s %s.", Monnam(mtmp),
                  (mtmp->data == &mons[PM_MEDUSA] &&
                   (mtmp->mcan || Hallucination)) ?
                  "doesn't look all that ugly" :
                  "gazes ineffectually");
            break;
        }
        if (Reflecting && couldsee(mtmp->mx, mtmp->my) &&
            mtmp->data == &mons[PM_MEDUSA]) {
            /* hero has line of sight to Medusa and she's not blind */
            boolean useeit = canseemon(mtmp);

            if (useeit)
                (void) ureflects("%s gaze is reflected by your %s.",
                                 s_suffix(Monnam(mtmp)));
            if (mon_reflects(mtmp, !useeit ? (char *)0 :
                             "The gaze is reflected away by %s %s!"))
                break;
            if (!m_canseeu(mtmp)) { /* probably you're invisible */
                if (useeit)
                    pline(
                        "%s doesn't seem to notice that %s gaze was reflected.",
                        Monnam(mtmp), mhis(mtmp));
                break;
            }
            if (useeit)
                pline("%s is turned to stone!", Monnam(mtmp));
            stoned = TRUE;
            killed(mtmp);

            if (mtmp->mhp > 0) break;
            return 2;
        }
        if (canseemon(mtmp) && couldsee(mtmp->mx, mtmp->my) &&
            !Stone_resistance) {
            You("meet %s gaze.", s_suffix(mon_nam(mtmp)));
            stop_occupation();
            if(poly_when_stoned(youmonst.data) && polymon(PM_STONE_GOLEM))
                break;
            urgent_pline("You turn to stone...");
            killer.format = KILLED_BY;
            Strcpy(killer.name, mtmp->data->mname);
            done(STONING);
        }
        break;

    case AD_CONF:
        if (canseemon(mtmp) &&
            couldsee(mtmp->mx, mtmp->my) &&
            mtmp->mcansee && !mtmp->mspec_used && rn2(5)) {
            if (cancelled) {
                react = 0; /* "confused" */
                already = (mtmp->mconf != 0);
            } else {
                int conf = d(3, 4);

                mtmp->mspec_used = mtmp->mspec_used + (conf + rn2(6));
                if (!Confusion) {
                    pline("%s gaze confuses you!", s_suffix(Monnam(mtmp)));
                } else {
                    You("are getting more and more confused.");
                }
                make_confused(HConfusion + conf, FALSE);
                stop_occupation();
            }
        }
        break;

    case AD_STUN:
        if (canseemon(mtmp) &&
           couldsee(mtmp->mx, mtmp->my) &&
           mtmp->mcansee && !mtmp->mspec_used && rn2(5)) {
            if (cancelled) {
                react = 1; /* "stunned" */
                already = (mtmp->mstun != 0);
            } else {
                int stun = d(2, 6);

                mtmp->mspec_used = mtmp->mspec_used + (stun + rn2(6));
                pline("%s stares piercingly at you!", Monnam(mtmp));
                make_stunned((HStun & TIMEOUT) + stun, TRUE);
                stop_occupation();
            }
        }
        break;

    case AD_BLND:
        if (canseemon(mtmp) && !resists_blnd(&youmonst) &&
             distu(mtmp->mx, mtmp->my) <= BOLT_LIM*BOLT_LIM) {
            if (cancelled) {
                react = rn1(2, 2); /* "puzzled" || "dazzled" */
                already = (mtmp->mcansee == 0);
                /* Archons gaze every round; we don't want cancelled ones
                   giving the "seems puzzled/dazzled" message that often */
                if (mtmp->mcan && mtmp->data == &mons[PM_ARCHON] && rn2(5)) {
                    react = -1;
                }
            } else {
                int blnd = d((int)mattk->damn, (int)mattk->damd);

                You("are blinded by %s radiance!",
                    s_suffix(mon_nam(mtmp)));
                make_blinded((long)blnd, FALSE);
                stop_occupation();
                /* not blind at this point implies you're wearing
                the Eyes of the Overworld; make them block this
                particular stun attack too */
                if (!Blind) {
                    Your("%s", vision_clears);
                } else {
                    long oldstun = (HStun & TIMEOUT), newstun = (long) rnd(3);

                    /* we don't want to increment stun duration every time
                       or sighted hero will become incapacitated */
                    make_stunned(max(oldstun, newstun), TRUE);
                }
            }
        }
        break;

    case AD_FIRE:
        if (canseemon(mtmp) &&
             couldsee(mtmp->mx, mtmp->my) &&
             mtmp->mcansee && !mtmp->mspec_used && rn2(5)) {
            if (cancelled) {
                react = rn1(2, 4); /* "irritated" || "inflamed" */
            } else {
                int dmg = d(2, 6);

                pline("%s attacks you with a fiery gaze!", Monnam(mtmp));
                stop_occupation();
                if (Fire_resistance) {
                    pline_The("fire doesn't feel hot!");
                    dmg = 0;
                }
                burn_away_slime();
                if ((int) mtmp->m_lev > rn2(20))
                    destroy_item(SCROLL_CLASS, AD_FIRE);
                if ((int) mtmp->m_lev > rn2(20))
                    destroy_item(POTION_CLASS, AD_FIRE);
                if ((int) mtmp->m_lev > rn2(25))
                    destroy_item(SPBOOK_CLASS, AD_FIRE);
                if (dmg) mdamageu(mtmp, dmg);
            }
        }
        break;

    case AD_BLNK:
        if (!mtmp->mcan && canseemon(mtmp) &&
            couldsee(mtmp->mx, mtmp->my) &&
            mtmp->mcansee && !mtmp->mspec_used && rn2(5)) {
            int dmg = d(1, 4);
            if (!Reflecting) {
                pline("%s reflection in your mind weakens you.", s_suffix(Monnam(mtmp)));
                stop_occupation();
                exercise(A_INT, TRUE);
            } else {
                if (flags.verbose)
                    /* Since this message means the player is unaffected, limit
                       its occurence to preserve flavor but avoid message spam */
                    if (!rn2(10)) pline("%s is covering its face.", Monnam(mtmp));
                dmg = 0;
            }
            if (dmg) mdamageu(mtmp, dmg);
        }
        break;

    case AD_LUCK:
        if(!mtmp->mcan && canseemon(mtmp) &&
           couldsee(mtmp->mx, mtmp->my) &&
           mtmp->mcansee && !mtmp->mspec_used && rn2(5)) {
            pline("%s glares ominously at you!", Monnam(mtmp));
            mtmp->mspec_used = mtmp->mspec_used + d(2, 6);

            if (uwep && uwep->otyp == MIRROR && uwep->blessed) {
                pline("%s sees its own glare in your mirror.",
                      Monnam(mtmp));
                pline("%s is cancelled!", Monnam(mtmp));
                mtmp->mcan = 1;
                monflee(mtmp, 0, FALSE, TRUE);
            } else if((uwep && !uwep->cursed && confers_luck(uwep)) ||
                      (stone_luck(TRUE) > 0 && rn2(4))) {
                pline("Luckily, you are not affected.");
            } else {
                You_feel("your luck running out.");
                change_luck(-1);
            }
            stop_occupation();
        }
        break;

    case AD_SPOR:
        /* release a spore if the player is nearby */
        if (is_fern(mtmp->data) && !mtmp->mcan && distu(mtmp->mx, mtmp->my) <= 96 &&
            !is_fern_sprout(mtmp->data) ? rn2(2) : !rn2(4)) {
            coord mm;
            mm.x = mtmp->mx; mm.y = mtmp->my;
            enexto(&mm, mm.x, mm.y, &mons[PM_FERN_SPORE]);
            if (mtmp->data == &mons[PM_ARCTIC_FERN] ||
                mtmp->data == &mons[PM_ARCTIC_FERN_SPROUT]) {
                makemon(&mons[PM_ARCTIC_FERN_SPORE], mm.x, mm.y, NO_MM_FLAGS);
            } else if (mtmp->data == &mons[PM_BLAZING_FERN] ||
                       mtmp->data == &mons[PM_BLAZING_FERN_SPROUT]) {
                makemon(&mons[PM_BLAZING_FERN_SPORE], mm.x, mm.y, NO_MM_FLAGS);
            } else if (mtmp->data == &mons[PM_DUNGEON_FERN] ||
                       mtmp->data == &mons[PM_DUNGEON_FERN_SPROUT]) {
                makemon(&mons[PM_DUNGEON_FERN_SPORE], mm.x, mm.y, NO_MM_FLAGS);
            } else if (mtmp->data == &mons[PM_SWAMP_FERN] ||
                       mtmp->data == &mons[PM_SWAMP_FERN_SPROUT]) {
                makemon(&mons[PM_SWAMP_FERN_SPORE], mm.x, mm.y, NO_MM_FLAGS);
            } else { /* currently these should not be generated */
                makemon(&mons[PM_FERN_SPORE], mm.x, mm.y, NO_MM_FLAGS);
            }
            if (canseemon(mtmp)) pline("%s releases a spore!", Monnam(mtmp));
        }
        break;

#ifdef PM_BEHOLDER /* work in progress */
    case AD_SLEE:
        if(!mtmp->mcan && canseemon(mtmp) &&
           couldsee(mtmp->mx, mtmp->my) && mtmp->mcansee &&
           multi >= 0 && !rn2(5) && !Sleep_resistance) {

            fall_asleep(-rnd(10), TRUE);
            pline("%s gaze makes you very sleepy...",
                  s_suffix(Monnam(mtmp)));
        }
        break;
    case AD_SLOW:
        if(!mtmp->mcan && canseemon(mtmp) && mtmp->mcansee &&
           (HFast & (INTRINSIC|TIMEOUT)) &&
           !defends(AD_SLOW, uwep) && !rn2(4))

            u_slow_down();
        stop_occupation();
        break;
#endif
    default: warning("Gaze attack %d?", mattk->adtyp);
        break;
    }
    if (react >= 0) {
        if (Hallucination && rn2(3)) {
            react = rn2(SIZE(reactions));
        }
        /* cancelled/hallucinatory feedback; monster might look "confused",
           "stunned",&c but we don't actually set corresponding attribute */
        pline("%s looks %s%s.", Monnam(mtmp),
                !rn2(3) ? "" :
                already ? "quite " :
                !rn2(2) ? "a bit " : "somewhat ",
                reactions[react]);
    }

    return(0);
}

/* mtmp hits you for n points damage */
void
mdamageu(struct monst *mtmp, int n)
{
    showdmg(n, TRUE);
    flags.botl = 1;
    if (Upolyd) {
        u.mh -= n;
        if (u.mh < 1) rehumanize();
    } else {
        u.uhp -= n;
        if(u.uhp < 1) done_in_by(mtmp);
    }
}

/* returns 0 if seduction impossible,
 *         1 if fine,
 *         2 if wrong gender for nymph
 */
int
could_seduce(
    struct monst *magr,
    struct monst *mdef,
    struct attack *mattk) /**< non NULL: current attack; NULL: general capability */
{
    struct permonst *pagr;
    boolean agrinvis, defperc;
    xint16 genagr, gendef;
    int adtyp;

    if (is_animal(magr->data)) return (0);
    if (magr == &youmonst) {
        pagr = youmonst.data;
        agrinvis = (Invis != 0);
        genagr = poly_gender();
    } else {
        pagr = magr->data;
        agrinvis = magr->minvis;
        genagr = gender(magr);
    }
    if (mdef == &youmonst) {
        defperc = (See_invisible != 0);
        gendef = poly_gender();
    } else {
        defperc = perceives(mdef->data);
        gendef = gender(mdef);
    }

    if(agrinvis && !defperc
#ifdef SEDUCE
       && mattk && mattk->adtyp != AD_SSEX
#endif
       )
        return 0;

    /* nymphs have two attacks, one for steal-item damage and the other
       for seduction, both pass the could_seduce() test;
       incubi/succubi have three attacks, their claw attacks for damage
       don't pass the test */
    if(pagr->mlet != S_NYMPH
       && ((pagr != &mons[PM_INCUBUS] && pagr != &mons[PM_SUCCUBUS])
#ifdef SEDUCE
           || (mattk && mattk->adtyp != AD_SSEX)
#endif
           ))
        return 0;

    if(genagr == 1 - gendef)
        return 1;
    else
        return (pagr->mlet == S_NYMPH) ? 2 : 0;
}

#ifdef SEDUCE
/* returns 1 if monster teleported (or hero leaves monster's vicinity) */
int
doseduce(struct monst *mon)
{
    struct obj *ring, *nring;
    boolean fem = (mon->data == &mons[PM_SUCCUBUS]); /* otherwise incubus */
    int tried_gloves = 0;
    char qbuf[QBUFSZ], Who[QBUFSZ];

    if (mon->mcan || mon->mspec_used) {
        pline("%s acts as though %s has got a %sheadache.",
              Monnam(mon), mhe(mon),
              mon->mcan ? "severe " : "");
        return 0;
    }

    if (unconscious()) {
        pline("%s seems dismayed at your lack of response.",
              Monnam(mon));
        return 0;
    }

    boolean seewho = canseemon(mon);
    if (!seewho) {
        pline("Someone caresses you...");
    } else {
        You_feel("very attracted to %s.", mon_nam(mon));
    }
    /* cache the seducer's name in a local buffer */
    Strcpy(Who, (!seewho ? (fem ? "She" : "He") : Monnam(mon)));

    /* if in the process of putting armor on or taking armor off,
       interrupt that activity now */
    (void) stop_donning((struct obj *) 0);
    /* don't try to take off gloves if cursed weapon blocks them */
    if (welded(uwep)) {
        tried_gloves = 1;
    }

    for (ring = invent; ring; ring = nring) {
        nring = ring->nobj;
        if (ring->otyp != RIN_ADORNMENT) continue;
        if (fem) {
            if (ring->owornmask && uarmg) {
                /* don't take off worn ring if gloves are in the way */
                if (!tried_gloves++) {
                    mayberem(mon, Who, uarmg, "gloves");
                }
                if (uarmg) {
                    continue; /* next ring might not be worn */
                }
            }
            /* confirmation prompt when charisma is high bypassed if deaf */
            if (!Deaf && (rn2(20) < ACURR(A_CHA))) {
                (void) safe_qbuf(qbuf, "\"That ",
                                 " looks pretty.  May I have it?\"", ring,
                                 xname, simpleonames, "ring");
                makeknown(RIN_ADORNMENT);
                if (yn(qbuf) == 'n') continue;
            } else {
                pline("%s decides she'd like %s, and takes it.", Who, yname(ring));
            }
            makeknown(RIN_ADORNMENT);
            /* might be in left or right ring slot or weapon/alt-wep/quiver */
            if (ring->owornmask) {
                remove_worn_item(ring, FALSE);
            }
            freeinv(ring);
            (void) mpickobj(mon, ring);
        } else {
            if (uleft && uright && uleft->otyp == RIN_ADORNMENT
                && uright->otyp==RIN_ADORNMENT)
                break;
            if (ring==uleft || ring==uright) continue;
            if (uarmg) {
                /* don't put on ring if gloves are in the way */
                if (!tried_gloves++) {
                    mayberem(mon, Who, uarmg, "gloves");
                }
                if (uarmg) {
                    break; /* no point trying further rings */
                }
            }
            /* confirmation prompt when charisma is high bypassed if deaf */
            if (!Deaf && (rn2(20) < ACURR(A_CHA))) {
                (void) safe_qbuf(qbuf, "\"That ",
                                 " looks pretty.  Would you wear it for me?\"",
                                 ring, xname, simpleonames, "ring");
                makeknown(RIN_ADORNMENT);
                if (yn(qbuf) == 'n') continue;
            } else {
                pline("%s decides you'd look prettier wearing %s,", Who, yname(ring));
                pline("and puts it on your finger.");
            }
            makeknown(RIN_ADORNMENT);
            if (!uright) {
                pline("%s puts %s on your right %s.",
                      Who, the(xname(ring)), body_part(HAND));
                setworn(ring, RIGHT_RING);
            } else if (!uleft) {
                pline("%s puts %s on your left %s.",
                      Who, the(xname(ring)), body_part(HAND));
                setworn(ring, LEFT_RING);
            } else if (uright && uright->otyp != RIN_ADORNMENT) {
                /* note: the "replaces" message might be inaccurate if
                   hero's location changes and the process gets interrupted,
                   but trying to figure that out in advance in order to use
                   alternate wording is not worth the effort */
                pline("%s replaces %s with %s.", Who, yname(uright), yname(ring));
                Ring_gone(uright);
                /* ring removal might cause loss of levitation which could
                   drop hero onto trap that transports hero somewhere else */
                if (u.utotype || distu(mon->mx, mon->my) > 2) {
                    return 1;
                }
                setworn(ring, RIGHT_RING);
            } else if (uleft && uleft->otyp != RIN_ADORNMENT) {
                /* see "replaces" note above */
                pline("%s replaces %s with %s.", Who, yname(uleft), yname(ring));
                Ring_gone(uleft);
                if (u.utotype || distu(mon->mx, mon->my) > 2) {
                    return 1;
                }
                setworn(ring, LEFT_RING);
            } else warning("ring replacement");
            Ring_on(ring);
            prinv((char *)0, ring, 0L);
        }
    }

    if (!uarmc && !uarmf && !uarmg && !uarms && !uarmh && !uarmu) {
        urgent_pline("%s murmurs sweet nothings into your ear.", Who);
    } else {
        urgent_pline("%s murmurs in your ear, while helping you undress.", Who);
    }
    mayberem(mon, Who, uarmc, cloak_simple_name(uarmc));
    if (!uarmc) {
        mayberem(mon, Who, uarm, suit_simple_name(uarm));
    }
    mayberem(mon, Who, uarmf, "boots");
    if (!tried_gloves) {
        mayberem(mon, Who, uarmg, "gloves");
    }
    mayberem(mon, Who, uarms, "shield");
    mayberem(mon, Who, uarmh, helm_simple_name(uarmh));
    if (!uarmc && !uarm) {
        mayberem(mon, Who, uarmu, "shirt");
    }

    /* removing armor (levitation boots, or levitation ring to make
       room for adornment ring with incubus case) might result in the
       hero falling through a trap door or landing on a teleport trap
       and changing location, so hero might not be adjacent to seducer
       any more (mayberem() has its own adjacency test so we don't need
       to check after each potential removal) */
    if (u.utotype || distu(mon->mx, mon->my) > 2) {
        return 1;
    }

    if (uarm || uarmc) {
        if (!Deaf) {
            verbalize("You're such a %s; I wish...",
                      flags.female ? "sweet lady" : "nice guy");
        } else if (seewho) {
            pline("%s appears to sigh.", Monnam(mon));
        }
        /* else no regret message if can't see or hear seducer */

        if (!tele_restrict(mon)) {
            (void) rloc(mon, TRUE);
        }
        return 1;
    }
    if (u.ualign.type == A_CHAOTIC)
        adjalign(1);

    /* by this point you have discovered mon's identity, blind or not... */
    urgent_pline("Time stands still while you and %s lie in each other's arms...",
          noit_mon_nam(mon));
    /* 3.6.1: a combined total for charisma plus intelligence of 35-1
       used to guarantee successful outcome; now total maxes out at 32
       as far as deciding what will happen; chance for bad outcome when
       Cha+Int is 32 or more is 2/35, a bit over 5.7% */
    int attr_tot = ACURR(A_CHA) + ACURR(A_INT);
    if (rn2(35) > min(attr_tot, 32)) {
        /* Don't bother with mspec_used here... it didn't get tired! */
        pline("%s seems to have enjoyed it more than you...",
              noit_Monnam(mon));
        switch (rn2(5)) {
        case 0: You_feel("drained of energy.");
            u.uen = 0;
            u.uenmax -= rnd(Half_physical_damage ? 5 : 10);
            exercise(A_CON, FALSE);
            if (u.uenmax < 0) u.uenmax = 0;
            break;

        case 1: You("are down in the dumps.");
            (void) adjattrib(A_CON, -1, TRUE);
            exercise(A_CON, FALSE);
            flags.botl = 1;
            break;

        case 2: Your("senses are dulled.");
            (void) adjattrib(A_WIS, -1, TRUE);
            exercise(A_WIS, FALSE);
            flags.botl = 1;
            break;

        case 3:
            if (!resists_drli(&youmonst)) {
                You_feel("out of shape.");
                losexp("overexertion");
            } else {
                You("have a curious feeling...");
            }
            exercise(A_CON, FALSE);
            exercise(A_DEX, FALSE);
            exercise(A_WIS, FALSE);
            break;

        case 4: {
            You_feel("exhausted.");
            exercise(A_STR, FALSE);
            int tmp = rn1(10, 6);
            losehp(Maybe_Half_Phys(tmp), "exhaustion", KILLED_BY);
            break;
        }
        }
    } else {
        mon->mspec_used = rnd(100); /* monster is worn out */
        You("seem to have enjoyed it more than %s...",
            noit_mon_nam(mon));
        switch (rn2(5)) {
        case 0: You_feel("raised to your full potential.");
            exercise(A_CON, TRUE);
            u.uen = (u.uenmax += rnd(5));
            break;

        case 1: You_feel("good enough to do it again.");
            (void) adjattrib(A_CON, 1, TRUE);
            exercise(A_CON, TRUE);
            flags.botl = 1;
            break;

        case 2: You("will always remember %s...", noit_mon_nam(mon));
            (void) adjattrib(A_WIS, 1, TRUE);
            exercise(A_WIS, TRUE);
            flags.botl = 1;
            break;

        case 3: pline("That was a very educational experience.");
            pluslvl(FALSE);
            exercise(A_WIS, TRUE);
            break;

        case 4: You_feel("restored to health!");
            u.uhp = u.uhpmax;
            if (Upolyd) u.mh = u.mhmax;
            exercise(A_STR, TRUE);
            flags.botl = 1;
            break;
        }
    }

    if (mon->mtame) /* don't charge */;
    else if (rn2(20) < ACURR(A_CHA)) {
        pline("%s demands that you pay %s, but you refuse...",
              noit_Monnam(mon), noit_mhim(mon));
    } else if (u.umonnum == PM_LEPRECHAUN)
        pline("%s tries to take your gold, but fails...",
              noit_Monnam(mon));
    else {
        long cost;
        long umoney = money_cnt(invent);

        if (umoney > (long)LARGEST_INT - 10L)
            cost = (long) rnd(LARGEST_INT) + 500L;
        else
            cost = (long) rnd((int)umoney + 10) + 500L;
        if (mon->mpeaceful) {
            cost /= 5L;
            if (!cost) cost = 1L;
        }
        if (cost > umoney) cost = umoney;
        if (!cost) verbalize("It's on the house!");
        else {
            pline("%s takes %ld %s for services rendered!",
                  noit_Monnam(mon), cost, currency(cost));
            money2mon(mon, cost);
            flags.botl = 1;
        }
    }
    if (!rn2(25)) mon->mcan = 1; /* monster is worn out */
    if (!tele_restrict(mon)) {
        (void) rloc(mon, TRUE);
    }

    /* After all has been said and done, try switching the gender of the foocubus.
     * The foocubus is searching for a new kind of victim.
     * https://reddit.com/comments/leuy6p//gms59yt */
    int mnum = (mon->data == &mons[PM_SUCCUBUS]) ? PM_INCUBUS : PM_SUCCUBUS;
    boolean show_msg = canseemon(mon);
    (void) newcham(mon, &mons[mnum], FALSE, show_msg);

    return 1;
}
#endif /* SEDUCE */

void
maybe_freeze_u(int *pdmg)
{
    if (Levitation || Flying) {
        if (pdmg) (*pdmg) = 0;
    } else if (is_lava(u.ux, u.uy)) {
        /* this block intentionally left blank */
    } else if (flaming(youmonst.data) ||
               (u.usteed && flaming(u.usteed->data))) {
        pline_The("ice melts away!");
        if (pdmg) (*pdmg) = 0;
    } else if (is_whirly(youmonst.data) ||
               amorphous(youmonst.data)) {
        pline_The("ice doesn't restrain you.");
        if (pdmg) (*pdmg) = 0;
    } else {
        if (levl[u.ux][u.uy].typ == POOL) {
            levl[u.ux][u.uy].typ = ICE;
            pline_The("water freezes!");
        }
        /* Other kinds of water let you escape from freezing. */
        else if (is_pool(u.ux, u.uy)) {
            if (pdmg) (*pdmg) = 0;
            return;
        }
        if (u.usteed) {
            pline_The("ice holds you and your %s "
                      "in place.", mon_nam(u.usteed));
        }
        else if (nolimbs(youmonst.data))
            pline_The("ice holds you in place.");
        else
            pline_The("ice holds your feet in place.");
        u.ufeetfrozen = max(rn1(16, 2), u.ufeetfrozen);
    }
}

#ifdef SEDUCE
static void
mayberem(struct monst *mon, const char *seducer, struct obj *obj, const char *str)

                     /* only used for alternate message */


{
    char qbuf[QBUFSZ];

    if (!obj || !obj->owornmask) return;
    /* removal of a previous item might have sent the hero elsewhere
       (loss of levitation that leads to landing on a transport trap) */
    if (u.utotype || distu(mon->mx, mon->my) > 2) {
        return;
    }

    /* being deaf overrides confirmation prompt for high charisma */
    if (Deaf) {
        pline("%s takes off your %s.", seducer, str);
    } else if (rn2(20) < ACURR(A_CHA)) {
        Sprintf(qbuf, "\"Shall I remove your %s, %s?\"",
                str,
                (rn2(2) ? "lover" : rn2(2) ? "dear" : "sweetheart"));
        if (yn(qbuf) == 'n') return;
    } else {
        char hairbuf[BUFSZ];

        Sprintf(hairbuf, "let me run my fingers through your %s",
                body_part(HAIR));
        verbalize("Take off your %s; %s.", str,
                  (obj == uarm)  ? "let's get a little closer" :
                  (obj == uarmc || obj == uarms) ? "it's in the way" :
                  (obj == uarmf) ? "let me rub your feet" :
                  (obj == uarmg) ? "they're too clumsy" :
                  (obj == uarmu) ? "let me massage you" :
                  /* obj == uarmh */
                  hairbuf);
    }
    remove_worn_item(obj, TRUE);
}
#endif  /* SEDUCE */

static int
mon_scream(struct monst *mtmp, struct attack *mattk)
{
    int effect = 0;
    int dmg = d(mattk->damn, mattk->damd);

    switch (mattk->adtyp) {
    case AD_STUN:
        /* don't let monsters use this every turn, or if not
           close, or if monster can see the player is asleep */
        if (mtmp->mspec_used || (m_canseeu(mtmp) && u.usleep) ||
            distu(mtmp->mx, mtmp->my) > 100) {
            return 0;
        }
        if (mtmp->mcan) {
            if (canseemon(mtmp)) {
                pline("%s croaks hoarsely.", Monnam(mtmp));
            } else {
                You_hear("a frog nearby.");
            }
        } else {
            if (canseemon(mtmp)) {
                pline("%s screams!", Monnam(mtmp));
            } else {
                if (distu(mtmp->mx, mtmp->my) > (dmg * 2)) dmg = 1;
                You_hear("a horrific scream!");
            }
            if (u.usleep) { unmul("You are shocked awake!"); } {
                Your("mind reels from the noise!");
                effect = 1;
            }
            make_stunned(HStun + dmg, FALSE);
        }
        mtmp->mspec_used = 2 + rn2(3) + dmg;
        break;
    default:
        break;
    }
    return effect;
}

/* FIXME:
 *  sequencing issue:  a monster's attack might cause poly'd hero
 *  to revert to normal form.  The messages for passive counterattack
 *  would look better if they came before reverting form, but we need
 *  to know whether hero reverted in order to decide whether passive
 *  damage applies.
 */
static int
passiveum(struct permonst *olduasmon, struct monst *mtmp, struct attack *mattk)
{
    int i, tmp;
    struct attack *oldu_mattk = 0;

    /*
     * mattk      == mtmp's attack that hit you;
     * oldu_mattk == your passive counterattack (even if mtmp's attack
     *               has already caused you to revert to normal form).
     */
    for (i = 0; !oldu_mattk; i++) {
        if (i >= NATTK) return 1;
        if (olduasmon->mattk[i].aatyp == AT_NONE ||
             olduasmon->mattk[i].aatyp == AT_BOOM) {
            oldu_mattk = &olduasmon->mattk[i];
        }
    }
    if (oldu_mattk->damn) {
        tmp = d((int) oldu_mattk->damn, (int) oldu_mattk->damd);
    } else if (oldu_mattk->damd) {
        tmp = d((int) olduasmon->mlevel + 1, (int) oldu_mattk->damd);
    } else {
        tmp = 0;
    }

    /* These affect the enemy even if you were "killed" (rehumanized) */
    switch (oldu_mattk->adtyp) {
    case AD_ACID:
        if (rn2(2)) {
            pline("%s is splashed by %s%s!", Monnam(mtmp),
                  /* temporary? hack for sequencing issue:  "your acid"
                     looks strange coming immediately after player has
                     been told that hero has reverted to normal form */
                  !Upolyd ? "" : "your ", hliquid("acid"));
            if (resists_acid(mtmp)) {
                pline("%s is not affected.", Monnam(mtmp));
                tmp = 0;
            }
        } else {
            tmp = 0;
        }
        if (!rn2(30)) {
            erode_armor(mtmp, ERODE_CORRODE);
        }
        if (!rn2(6)) {
            acid_damage(MON_WEP(mtmp));
        }
        goto assess_dmg;

    case AD_STON: /* cockatrice */
    {
        long protector = attk_protection((int)mattk->aatyp),
             wornitems = mtmp->misc_worn_check;

        /* wielded weapon gives same protection as gloves here */
        if (MON_WEP(mtmp) != 0) wornitems |= W_ARMG;

        if (!resists_ston(mtmp) && (protector == 0L ||
                                    (protector != ~0L &&
                                     (wornitems & protector) != protector))) {
            if (poly_when_stoned(mtmp->data)) {
                mon_to_stone(mtmp);
                return (1);
            }
            pline("%s turns to stone!", Monnam(mtmp));
            stoned = 1;
            xkilled(mtmp, XKILL_NOMSG);
            if (!DEADMONSTER(mtmp)) {
                return 1;
            }
            return 2;
        }
        return 1;
    }

    case AD_ENCH: /* KMH -- remove enchantment (disenchanter) */
        if (mon_currwep) {
            /* by_you==True: passive counterattack to hero's action
               is hero's fault */
            (void) drain_item(mon_currwep, TRUE);
            /* No message */
        }
        return (1);
    default:
        break;
    }
    if (!Upolyd) return 1;

    /* These affect the enemy only if you are still a monster */
    if (rn2(3)) {
        switch (oldu_mattk->adtyp) {
        case AD_PHYS:
            if (oldu_mattk->aatyp == AT_BOOM) {
                You("explode!");
                /* KMH, balance patch -- this is okay with unchanging */
                rehumanize();
                goto assess_dmg;
            }
            break;

        case AD_PLYS: /* Floating eye */
            if (tmp > 127) tmp = 127;
            if (u.umonnum == PM_FLOATING_EYE) {
                if (!rn2(4)) tmp = 127;
                if (mtmp->mcansee && haseyes(mtmp->data) && rn2(3) &&
                    (perceives(mtmp->data) || !Invis)) {
                    if (Blind)
                        pline("As a blind %s, you cannot defend yourself.",
                              youmonst.data->mname);
                    else {
                        if (mon_reflects(mtmp,
                                         "Your gaze is reflected by %s %s."))
                            return 1;
                        pline("%s is frozen by your gaze!", Monnam(mtmp));
                        paralyze_monst(mtmp, tmp);
                        return 3;
                    }
                }
            } else { /* gelatinous cube */
                pline("%s is frozen by you.", Monnam(mtmp));
                paralyze_monst(mtmp, tmp);
                return 3;
            }
            return 1;

        case AD_COLD: /* Brown mold or blue jelly */
            if (resists_cold(mtmp)) {
                shieldeff(mtmp->mx, mtmp->my);
                pline("%s is mildly chilly.", Monnam(mtmp));
                golemeffects(mtmp, AD_COLD, tmp);
                tmp = 0;
                break;
            }
            pline("%s is suddenly very cold!", Monnam(mtmp));
            u.mh += tmp / 2;
            if (u.mhmax < u.mh) set_uhpmax(u.mh, TRUE);
            if (u.mhmax > ((youmonst.data->mlevel+1) * 8))
                (void)split_mon(&youmonst, mtmp);
            break;

        case AD_STUN: /* Yellow mold */
            if (!mtmp->mstun) {
                mtmp->mstun = 1;
                pline("%s %s.", Monnam(mtmp),
                      makeplural(stagger(mtmp->data, "stagger")));
            }
            tmp = 0;
            break;

        case AD_FIRE: /* Red mold */
            if (resists_fire(mtmp)) {
                shieldeff(mtmp->mx, mtmp->my);
                pline("%s is mildly warm.", Monnam(mtmp));
                golemeffects(mtmp, AD_FIRE, tmp);
                tmp = 0;
                break;
            }
            pline("%s is suddenly very hot!", Monnam(mtmp));
            break;

        case AD_ELEC:
            if (resists_elec(mtmp)) {
                shieldeff(mtmp->mx, mtmp->my);
                pline("%s is slightly tingled.", Monnam(mtmp));
                golemeffects(mtmp, AD_ELEC, tmp);
                tmp = 0;
                break;
            }
            pline("%s is jolted with your electricity!", Monnam(mtmp));
            break;
        default: tmp = 0;
            break;
        }
    } else {
        tmp = 0;
    }

assess_dmg:
    if((mtmp->mhp -= tmp) <= 0) {
        pline("%s dies!", Monnam(mtmp));
        xkilled(mtmp, XKILL_NOMSG);
        if (!DEADMONSTER(mtmp)) {
            return 1;
        }
        return 2;
    }
    return 1;
}

static void
invulnerability_messages(struct monst *mtmp, boolean range2, boolean youseeit)
{
    /* monsters won't attack you */
    if(mtmp == u.ustuck)
        pline("%s loosens its grip slightly.", Monnam(mtmp));
    else if(!range2) {
        if (youseeit || sensemon(mtmp))
            pline("%s starts to attack you, but pulls back.",
                  Monnam(mtmp));
        else
            You_feel("%s move nearby.", something);
    }
}

struct monst *
cloneu(void)
{
    struct monst *mon;
    int mndx = monsndx(youmonst.data);

    if (u.mh <= 1) return (struct monst *)0;
    if (mvitals[mndx].mvflags & G_EXTINCT) return (struct monst *)0;
    mon = makemon(youmonst.data, u.ux, u.uy, NO_MINVENT|MM_EDOG);
    if (!mon) {
        return NULL;
    }
    mon->mcloned = 1;
    mon = christen_monst(mon, plname);
    initedog(mon);
    mon->m_lev = youmonst.data->mlevel;
    mon->mhpmax = u.mhmax;
    mon->mhp = u.mh / 2;
    u.mh -= mon->mhp;
    flags.botl = 1;

    return(mon);
}

/*mhitu.c*/
