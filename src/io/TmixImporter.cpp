#include "TmixImporter.h"
#include "core/Actor.h"
#include "core/ActorProfileLibrary.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/CueZero.h"
#include "core/DCAMapping.h"
#include "core/Ensemble.h"
#include "core/Position.h"
#include "core/Show.h"

#include <QHash>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

namespace OpenMix {

namespace {

QList<int> parseIntList(const QString& text) {
    QList<int> out;
    for (const QString& tok : text.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts)) {
        bool ok = false;
        int v = tok.toInt(&ok);
        if (ok)
            out.append(v);
    }
    return out;
}

// parse a compact per-channel map: "ch=value,ch=value" (also accepts ':' and ';')
QList<QPair<int, QString>> parsePairs(const QString& text) {
    QList<QPair<int, QString>> out;
    for (const QString& tok : text.split(QRegularExpression("[,;]+"), Qt::SkipEmptyParts)) {
        const QStringList kv = tok.split(QRegularExpression("[=:]"));
        if (kv.size() >= 2) {
            bool ok = false;
            int ch = kv.at(0).trimmed().toInt(&ok);
            if (ok)
                out.append({ch, kv.at(1).trimmed()});
        }
    }
    return out;
}

bool truthy(const QString& v) {
    return v == "1" || v.compare("true", Qt::CaseInsensitive) == 0;
}

QString dcaColumn(int dca, const char* suffix) {
    return QString("dca%1%2").arg(dca, 2, 10, QChar('0')).arg(suffix);
}

void clearShow(Show* show) {
    show->cueList()->clear();
    show->actorProfileLibrary()->clear();
    show->positionLibrary()->clear();
    show->cueZero()->clear();
    show->dcaMapping()->clear();
    const auto ensembles = show->ensembleLibrary()->ensembles();
    for (const Ensemble& e : ensembles)
        show->ensembleLibrary()->removeEnsemble(e.id());
}

} // namespace

bool TmixImporter::import(const QString& path, Show* show, QString* error,
                          TmixImportSummary* summary) {
    if (!show)
        return false;

    static int s_counter = 0;
    const QString connName = QStringLiteral("tmiximport_%1").arg(++s_counter);

    bool ok = true;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(path);
        if (!db.open()) {
            if (error)
                *error = db.lastError().text();
            ok = false;
        } else {
            clearShow(show);

            QSqlQuery q(db);

            // positions: keep a map from the file's position id to the new id
            QHash<int, QString> posIdMap;
            if (q.exec("SELECT * FROM positions")) {
                while (q.next()) {
                    const QSqlRecord r = q.record();
                    Position p(r.value("name").toString(), r.value("shortName").toString());
                    p.setDelay(r.value("delay").toDouble());
                    p.setPan(r.value("pan").toDouble());
                    if (r.indexOf("buses") >= 0)
                        p.setBuses(parseIntList(r.value("buses").toString()));
                    const QString newId = show->positionLibrary()->addPosition(p);
                    if (r.indexOf("id") >= 0)
                        posIdMap.insert(r.value("id").toInt(), newId);
                    if (summary)
                        ++summary->positions;
                }
            }

            // channel voice presets become profile slots; map file id to slot name
            QHash<int, QString> profileSlotMap;
            if (q.exec("SELECT * FROM profiles")) {
                while (q.next()) {
                    const QSqlRecord r = q.record();
                    QString label = r.value("label").toString();
                    if (label.isEmpty())
                        label = r.value("name").toString();
                    if (!label.isEmpty()) {
                        show->actorProfileLibrary()->addSlot(label);
                        if (summary)
                            summary->profileSlots.append(label);
                    }
                    if (r.indexOf("id") >= 0)
                        profileSlotMap.insert(r.value("id").toInt(), label);
                }
            }

            // actors
            if (q.exec("SELECT * FROM actors")) {
                while (q.next()) {
                    const QSqlRecord r = q.record();
                    Actor a(r.value("name").toString(), r.value("channel").toInt());
                    a.setOrder(r.value("order").toInt());
                    a.setActive(r.value("active").toInt() != 0);
                    show->actorProfileLibrary()->addActor(a);
                    if (summary)
                        ++summary->actors;
                }
            }

            // ensembles
            if (q.exec("SELECT * FROM ensembles")) {
                while (q.next()) {
                    const QSqlRecord r = q.record();
                    Ensemble e(r.value("name").toString());
                    e.setChannels(parseIntList(r.value("channels").toString()));
                    show->ensembleLibrary()->addEnsemble(e);
                    if (summary)
                        ++summary->ensembles;
                }
            }

            // cues; along the way, guess actor roles from DCA labels: a label
            // on a single-channel DCA names whoever is on that channel (multi-
            // channel labels are group names, conflicting labels are skipped)
            QHash<int, QString> channelRoleGuess;
            if (q.exec("SELECT * FROM cues")) {
                while (q.next()) {
                    const QSqlRecord r = q.record();
                    const double number =
                        r.value("number").toDouble() + r.value("point").toDouble() / 10.0;
                    Cue cue(number, r.value("name").toString());

                    QMap<int, QList<int>> dcaMap;
                    for (int i = 1; i <= 8; ++i) {
                        const QString cc = dcaColumn(i, "Channels");
                        if (r.indexOf(cc) >= 0) {
                            const QList<int> chs = parseIntList(r.value(cc).toString());
                            if (!chs.isEmpty())
                                dcaMap.insert(i, chs);
                        }
                        const QString lc = dcaColumn(i, "Label");
                        if (r.indexOf(lc) >= 0) {
                            const QString label = r.value(lc).toString();
                            if (!label.isEmpty()) {
                                DCAOverride ov;
                                ov.label = label;
                                cue.setDCAOverride(i, ov);

                                if (dcaMap.value(i).size() == 1) {
                                    const int ch = dcaMap.value(i).first();
                                    const QString prior = channelRoleGuess.value(ch);
                                    if (!channelRoleGuess.contains(ch))
                                        channelRoleGuess.insert(ch, label);
                                    else if (prior.compare(label, Qt::CaseInsensitive) != 0)
                                        channelRoleGuess.insert(ch, QString()); // ambiguous
                                }
                            }
                        }
                    }
                    if (!dcaMap.isEmpty())
                        cue.setDCAChannelMapping(dcaMap);

                    if (r.indexOf("channelPositions") >= 0) {
                        for (const auto& pr : parsePairs(r.value("channelPositions").toString())) {
                            bool pok = false;
                            int pid = pr.second.toInt(&pok);
                            if (pok && posIdMap.contains(pid))
                                cue.setChannelPosition(pr.first, posIdMap.value(pid));
                        }
                    }

                    if (r.indexOf("channelProfiles") >= 0) {
                        for (const auto& pr : parsePairs(r.value("channelProfiles").toString())) {
                            bool prok = false;
                            int prid = pr.second.toInt(&prok);
                            const QString slot = (prok && profileSlotMap.contains(prid))
                                                     ? profileSlotMap.value(prid)
                                                     : pr.second;
                            if (!slot.isEmpty())
                                cue.setChannelProfile(pr.first, slot);
                        }
                    }

                    if (r.indexOf("fxMutes") >= 0) {
                        for (const auto& pr : parsePairs(r.value("fxMutes").toString()))
                            cue.setFxMute(pr.first, truthy(pr.second));
                    }

                    show->cueList()->addCue(cue);
                    if (summary)
                        ++summary->cues;
                }
            }

            // apply the unambiguous role guesses to actors without one
            // (iterate a copy: updateActor mutates the library's list)
            const QList<Actor> actors = show->actorProfileLibrary()->actors();
            for (const Actor& a : actors) {
                const QString guess = channelRoleGuess.value(a.channel());
                if (guess.isEmpty() || !a.role().isEmpty())
                    continue;
                Actor copy = a;
                copy.setRole(guess);
                show->actorProfileLibrary()->updateActor(a.id(), copy);
                if (summary)
                    ++summary->rolesInferred;
            }

            // config parameters mapped onto Show fields
            QHash<QString, QString> cfg;
            if (q.exec("SELECT param, value FROM config")) {
                while (q.next())
                    cfg.insert(q.value(0).toString(), q.value(1).toString());
            }
            if (cfg.contains("designer"))
                show->setDesigner(cfg.value("designer"));
            if (cfg.contains("dimDCAFaders"))
                show->setDimDcaFaders(truthy(cfg.value("dimDCAFaders")));
            if (cfg.contains("selectOnSpill"))
                show->setSelectOnSpill(truthy(cfg.value("selectOnSpill")));
            if (cfg.contains("consoleMuteDCAUnassign"))
                show->setMuteDcaUnassign(truthy(cfg.value("consoleMuteDCAUnassign")));
            if (cfg.contains("suppressDCAMuteBackupSwitch"))
                show->setSuppressBackupSwitch(truthy(cfg.value("suppressDCAMuteBackupSwitch")));
            if (truthy(cfg.value("gangLR"))) {
                const QList<int> lr = parseIntList(cfg.value("gangLRChannels"));
                if (lr.size() >= 2)
                    show->setChannelGangs({qMakePair(lr.at(0), lr.at(1))});
            }
            if (cfg.contains("gangLRName"))
                show->setGangName(0, cfg.value("gangLRName"));
            if (cfg.contains("gangLRColour"))
                show->setGangColor(0, cfg.value("gangLRColour"));

            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return ok;
}

} // namespace OpenMix
