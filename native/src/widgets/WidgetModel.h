#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QUrl>
#include <QVariantMap>

namespace FlappedEar {

struct WidgetData {
    QString id;
    QString type;
    double x = 0.04;
    double y = 0.05;
    double width = 0.15;
    double height = 0.16;
    double scale = 1.0;
    double rotation = 0.0;
    double opacity = 1.0;
    bool visible = true;
    QVariantMap settings;
    QVariantList cues;
    QString groupId;
};

class WidgetModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(QVariantList templates READ templates NOTIFY templatesChanged)

public:
    enum Role {
        WidgetIdRole = Qt::UserRole + 1,
        WidgetTypeRole,
        WidgetXRole,
        WidgetYRole,
        WidgetWidthRole,
        WidgetHeightRole,
        WidgetScaleRole,
        WidgetRotationRole,
        WidgetOpacityRole,
        WidgetVisibleRole,
        WidgetSettingsRole,
        WidgetCuesRole,
        WidgetGroupIdRole,
    };

    explicit WidgetModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const;
    [[nodiscard]] int revision() const;
    [[nodiscard]] QVariantList templates() const;
    [[nodiscard]] const WidgetData *widgetAt(int index) const;

    Q_INVOKABLE int addWidget(const QString &type);
    Q_INVOKABLE void removeWidget(int index);
    Q_INVOKABLE void removeWidgets(const QVariantList &indices);
    Q_INVOKABLE int duplicateWidget(int index);
    Q_INVOKABLE void moveWidget(int index, double x, double y);
    Q_INVOKABLE void resizeWidget(int index, double width, double height);
    Q_INVOKABLE void setWidgetProperty(int index, const QString &name, const QVariant &value);
    Q_INVOKABLE void setSetting(int index, const QString &name, const QVariant &value);
    Q_INVOKABLE int addCue(int index, double startTime, double duration, const QString &effect);
    Q_INVOKABLE void setCueProperty(int index, int cueIndex, const QString &name, const QVariant &value);
    Q_INVOKABLE void removeCue(int index, int cueIndex);
    Q_INVOKABLE void clearCues(int index);
    Q_INVOKABLE QString groupWidgets(const QVariantList &indices);
    Q_INVOKABLE void ungroupWidget(int index);
    Q_INVOKABLE QVariantList groupMembers(int index) const;
    Q_INVOKABLE QVariantMap widget(int index) const;
    Q_INVOKABLE void resetDefaults();
    Q_INVOKABLE bool applyTemplate(const QString &templateId);
    Q_INVOKABLE QString saveCurrentAsTemplate(const QString &name, const QString &description);
    Q_INVOKABLE bool updateTemplate(const QString &templateId);
    Q_INVOKABLE bool deleteTemplate(const QString &templateId);
    Q_INVOKABLE bool exportTemplate(const QString &templateId, const QUrl &url) const;
    Q_INVOKABLE QString importTemplate(const QUrl &url);
    Q_INVOKABLE void reloadTemplates();

    [[nodiscard]] QJsonArray toJson() const;
    bool fromJson(const QJsonArray &array);

signals:
    void countChanged();
    void revisionChanged();
    void templatesChanged();

private:
    static WidgetData createWidget(const QString &type, int index);
    static bool validType(const QString &type);
    void update(int index);
    void loadUserTemplates();
    bool saveUserTemplates() const;
    [[nodiscard]] QJsonArray allTemplates() const;

    QList<WidgetData> m_widgets;
    QJsonArray m_userTemplates;
    int m_revision = 0;
};

} // namespace FlappedEar
