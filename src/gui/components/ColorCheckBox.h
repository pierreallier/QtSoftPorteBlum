#pragma once
/** @file
 * @brief Cases à cocher colorées pour les courbes et les axes de mesure.
 */
#include <QCheckBox>
#include <QColor>

class QPainter;

/**
 * @brief QCheckBox dont l'indicateur coloré est dessiné avec QPainter.
 *
 * La couleur définit la bordure et le fond lorsque la case est cochée. Le
 * libellé conserve la palette Qt, les mnémoniques et le sens de lecture.
 * Toute la surface du widget est cliquable. Un contour de focus est affiché
 * après navigation par Tab/Maj+Tab, puis masqué lors d'un appui souris.
 *
 * Les propriétés Qt permettent le réglage par QSS avec qproperty-* : color,
 * disabledColor, indicatorSize, indicatorBorderWidth, indicatorRadius et
 * checkWidth. Les dimensions sont exprimées en coordonnées logiques du widget.
 */
class ColorCheckBox : public QCheckBox {
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor)
    Q_PROPERTY(QColor disabledColor READ disabledColor WRITE setDisabledColor)
    Q_PROPERTY(qreal indicatorBorderWidth MEMBER indicatorBorderWidth_)
    Q_PROPERTY(qreal indicatorRadius MEMBER indicatorRadius_)
    Q_PROPERTY(qreal checkWidth MEMBER checkWidth_)
    Q_PROPERTY(int indicatorSize READ indicatorSize WRITE setIndicatorSize)
public:
    /// Construit une case avec libellé et couleur ; parent en devient propriétaire selon les règles Qt.
    explicit ColorCheckBox(const QString &text, const QColor &color, QWidget *parent = nullptr);
    /// Couleur de l'indicateur lorsque le widget est actif.
    QColor color() const { return color_; }
    /// Change la couleur et demande un rafraîchissement ; ignore une couleur invalide.
    void setColor(const QColor &color);
    /// Couleur désactivée configurée ; une couleur invalide signifie « utiliser la palette Qt ».
    QColor disabledColor() const { return disabledColor_; }
    /// Configure la bordure et le fond cochés à l'état désactivé ; invalide rétablit le repli sur la palette.
    void setDisabledColor(const QColor &color);
    /// Taille du carré réservé à l'indicateur, avant son retrait intérieur de dessin.
    int indicatorSize() const { return indicatorSize_; }
    /// Impose une taille d'au moins 1 et actualise le dessin et la géométrie du layout.
    void setIndicatorSize(int size);
    /// Taille conseillée calculée après application du QSS, selon l'indicateur, la police et le libellé.
    QSize sizeHint() const override;
    /// Taille minimale identique à la taille conseillée pour éviter de tronquer le contenu.
    QSize minimumSizeHint() const override { return sizeHint(); }
protected:
    /// Mémorise si le focus provient de Tab/Maj+Tab avant de déléguer à QCheckBox.
    void focusInEvent(QFocusEvent *event) override;
    /// Retire le contour de focus clavier puis conserve le traitement souris de QCheckBox.
    void mousePressEvent(QMouseEvent *event) override;
    /// Dessine l'indicateur, le symbole, le libellé et le contour de focus éventuel.
    void paintEvent(QPaintEvent *) override;
    /// Accepte comme zone d'activation tout point situé dans rect().
    bool hitButton(const QPoint &position) const override;
    /**
     * @brief Dessine par défaut une coche, uniquement lorsque la case est cochée.
     * Point d'extension utilisé par AxisColorCheckBox pour remplacer la coche.
     * @param painter Peintre du widget, avec anticrénelage actif.
     * @param area Rectangle intérieur de l'indicateur.
     * @param ink Couleur proposée, contrastée avec le fond coché.
     */
    virtual void paintSymbol(QPainter &painter, const QRectF &area, const QColor &ink) const;
private:
    bool tabFocus_ = false; ///< Origine clavier du focus, utilisée pour dessiner son contour.
    QColor color_; ///< Couleur active de la bordure et du fond coché.
    QColor disabledColor_; ///< Couleur désactivée facultative ; repli sur WindowText désactivé de la palette.
    qreal indicatorBorderWidth_ = 1.3; ///< Épaisseur de bordure, réglable par QSS.
    qreal indicatorRadius_ = 2; ///< Rayon des coins de l'indicateur, réglable par QSS.
    qreal checkWidth_ = 1.6; ///< Épaisseur du trait de la coche, réglable par QSS.
    int indicatorSize_ = 16; ///< Côté du carré réservé à l'indicateur.
};

/**
 * @brief Variante compacte affichant le nom d'un axe à la place de la coche.
 *
 * Le symbole reste visible lorsque la case est décochée. Aucun libellé extérieur
 * n'est ajouté ; le nom d'axe sert aussi de nom accessible. Les propriétés QSS
 * disabledSymbolColor, uncheckedSymbolColor et symbolBold règlent son apparence.
 */
class AxisColorCheckBox final : public ColorCheckBox {
    Q_OBJECT
    Q_PROPERTY(QColor disabledSymbolColor MEMBER disabledSymbolColor_)
    Q_PROPERTY(QColor uncheckedSymbolColor MEMBER uncheckedSymbolColor_)
    Q_PROPERTY(bool symbolBold MEMBER symbolBold_)
public:
    /// Construit un indicateur d'axe avec politiques de taille fixes dans les deux directions.
    explicit AxisColorCheckBox(const QString &axis, const QColor &color, QWidget *parent = nullptr);
protected:
    /// Centre l'axe dans l'indicateur ; sa police dépend de indicatorSize() et de symbolBold.
    /// Utilise uncheckedSymbolColor si décoché, sinon ink ou disabledSymbolColor selon l'activation.
    void paintSymbol(QPainter &painter, const QRectF &area, const QColor &ink) const override;
private:
    QColor disabledSymbolColor_; ///< Couleur du symbole coché désactivé ; repli sur la palette si invalide.
    QColor uncheckedSymbolColor_{"#9CA3AF"}; ///< Couleur du symbole décoché, même lorsque le widget est désactivé.
    bool symbolBold_ = true; ///< Dessine le symbole en gras lorsque vrai.
    QString axis_; ///< Texte de l'axe dessiné au centre de l'indicateur.
};
