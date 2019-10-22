#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QLineEdit>

#include <nodes/NodeDataModel>

#include <iostream>

class DecimalData;

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDataModel;
using QtNodes::NodeValidationState;

/// The model dictates the number of inputs and outputs for the Node.
/// In this example it has no logic.
class NumberSourceDataModel
  : public NodeDataModel
{
  Q_OBJECT

public:
  NumberSourceDataModel();

  virtual
  ~NumberSourceDataModel() {}

public:

  QString
  caption() const override
  { return QStringLiteral("Number Source"); }

  bool
  captionVisible() const override
  { return false; }

  QString
  name() const override
  { return QStringLiteral("NumberSource"); }

public:

  QJsonObject
  save() const override;

  void
  restore(QJsonObject const &p) override;

public:

  unsigned int
  nPorts(PortType portType) const override;

  NodeDataType
  dataType(PortType portType, PortIndex portIndex) const override;

  std::shared_ptr<NodeData>
  outData(PortIndex port) override;

  void
  setInData(std::shared_ptr<NodeData>, int) override
  { }

  QWidget *
  embeddedWidget() override { return _lineEdit; }

private Q_SLOTS:

  void
  onTextEdited(QString const &string);

private:

  std::shared_ptr<DecimalData> _number;

  QLineEdit * _lineEdit;
};


class DataData : public NodeData
{
public:

    DataData() : _number(0.0)
    {}

    DataData(double const number) : _number(number)
    {}

    NodeDataType type() const override
    {
        return NodeDataType{ "decimal", "Decimal" };
    }

    double number() const
    {
        return _number;
    }

    QString numberAsText() const
    {
        return QString::number(_number, 'f');
    }

private:

    double _number;
};

class DataSource : public NumberSourceDataModel
{
    Q_OBJECT
public:
    QString internalName;
    QList<QString> internalPorts;
    QList<QString> internalTypes;
    QList<QString> internalDefaults;
    QWidget*       internalWidget;
    QList<std::shared_ptr<NodeData>> internalData;

    QList<QString> internalPortsIn;
    QList<QString> internalTypesIn;
    QList<QString> internalDefaultsIn;
    QList<std::shared_ptr<NodeData>> internalDataIn;

    QList<QString> internalPortsOut;
    QList<QString> internalTypesOut;
    QList<QString> internalDefaultsOut;
    QList<std::shared_ptr<NodeData>> internalDataOut;

    QString name() const override
    {
        return internalName;
    }

    PortIndex find_port(QString name, QString type, PortType portType)
    {
        for (int i = 0; i < internalPorts.size(); i++)
        {
            if (internalPorts[i] == name)
                return i;
        }

        if (portType == PortType::In)
        {
            for (int i = 0; i < internalPortsIn.size(); i++)
            {
                if (internalPortsIn[i] == name)
                    return internalPorts.size() + i;
            }
        }

        if (portType == PortType::Out)
        {
            for (int i = 0; i < internalPortsOut.size(); i++)
            {
                if (internalPortsOut[i] == name)
                    return internalPorts.size() + i;
            }
        }

        return -1;
    }

    unsigned int nPorts(PortType portType) const override
    {

        if (portType == PortType::In)
        {
            return internalPorts.size() + internalPortsIn.size();
        }
        else if (portType == PortType::Out)
        {
            return internalPorts.size() + internalPortsOut.size();
        }
    }

    NodeDataType dataType(PortType portType, PortIndex portIndex) const override;
    std::shared_ptr<NodeData> outData(PortIndex port) override;
    QWidget * embeddedWidget() override { return internalWidget; }

    QString caption() const override
    {
        return internalName;
    }

    bool captionVisible() const override
    {
        return true;
    }

};
