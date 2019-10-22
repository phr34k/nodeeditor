#include <nodes/NodeData>
#include <nodes/FlowScene>
#include <nodes/FlowView>
#include <nodes/ConnectionStyle>
#include <nodes/TypeConverter>
#include <nodes/internal/Node.hpp>
#include <nodes/internal/NodeGraphicsObject.hpp>

#include <QtWidgets/QApplication>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QSlider>
#include <QFileDialog>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>


#include <nodes/DataModelRegistry>

#include "NumberSourceDataModel.hpp"
#include "NumberDisplayDataModel.hpp"
#include "AdditionModel.hpp"
#include "SubtractionModel.hpp"
#include "MultiplicationModel.hpp"
#include "DivisionModel.hpp"
#include "ModuloModel.hpp"
#include "Converters.hpp"
#include "tinyxml.h"


using QtNodes::DataModelRegistry;
using QtNodes::FlowScene;
using QtNodes::FlowView;
using QtNodes::ConnectionStyle;
using QtNodes::TypeConverter;
using QtNodes::TypeConverterId;

class FlowXmlScene : public FlowScene
{
public:
    FlowXmlScene(std::shared_ptr<DataModelRegistry> registry, QObject * parent = Q_NULLPTR) : FlowScene(registry, parent) { }
    FlowXmlScene(QObject * parent = Q_NULLPTR) : FlowScene(parent) { }

    void saveXml() const;
    void loadXml();
    QByteArray saveXmlToMemory() const;
    void loadXmlFromMemory(const QByteArray& data);
};



void FlowXmlScene::saveXml() const
{
    QString fileName =
        QFileDialog::getSaveFileName(nullptr,
            tr("Open Flow Scene"),
            QDir::homePath(),
            tr("Flow Scene Files (*.flow)"));

    if (!fileName.isEmpty())
    {
        if (!fileName.endsWith("flow", Qt::CaseInsensitive))
            fileName += ".flow";

        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly))
        {
            file.write(saveXmlToMemory());
        }
    }
}

void FlowXmlScene::loadXml()
{
    clearScene();

    //-------------

    QString fileName =
        QFileDialog::getOpenFileName(nullptr,
            tr("Open Flow Scene"),
            QDir::homePath(),
            tr("Flow Scene Files (*.flow)"));

    if (!QFileInfo::exists(fileName))
        return;

    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray wholeFile = file.readAll();

    loadXmlFromMemory(wholeFile);
}

QByteArray FlowXmlScene::saveXmlToMemory() const
{
    
    TiXmlDocument document;
    TiXmlElement* root = new TiXmlElement("Flow");
    root->SetAttribute("Version", "2");
    document.LinkEndChild(root);


    for (auto const & pair : _nodes)
    {           
        TiXmlElement* node = new TiXmlElement("Node");
        node->SetAttribute("Id", pair.first.toString().toLatin1().constData());
        node->SetAttribute("Type", pair.second->nodeDataModel()->name().toLatin1().constData());
        node->SetAttribute("Designer.Name", pair.first.toString().toLatin1().constData());
        node->SetAttribute("Designer.OffsetX", pair.second->nodeGraphicsObject().pos().x());
        node->SetAttribute("Designer.OffsetY", pair.second->nodeGraphicsObject().pos().y());
        root->LinkEndChild(node);

        DataSource* source = static_cast<DataSource*>(pair.second->nodeDataModel());
        for (int i = 0; i < source->internalPorts.size(); ++i)
        {
            TiXmlElement* property = new TiXmlElement("Property");
            property->SetAttribute("Name", source->internalPorts[i].toLatin1().constData());
            property->SetAttribute("Type", source->internalTypes[i].toLatin1().constData());
            property->SetAttribute("Value", source->internalDefaults[i].toLatin1().constData());
            node->LinkEndChild(property);
        }
    }

    for (auto const & pair : _connections)
    {
        QtNodes::Node* source = pair.second->getNode(PortType::Out);
        QtNodes::Node* target = pair.second->getNode(PortType::In);
        int sourceIndex = pair.second->getPortIndex(PortType::Out);
        int targetIndex = pair.second->getPortIndex(PortType::In);
        DataSource* sourceModel = static_cast<DataSource*>(source->nodeDataModel());
        DataSource* targetModel = static_cast<DataSource*>(target->nodeDataModel());
        
        QString sourcePart = sourceModel->internalPorts[sourceIndex];
        QString targetPart = targetModel->internalPorts[targetIndex];


        TiXmlElement* node = new TiXmlElement("Connection");      
        node->SetAttribute("Source", (target->id().toString() + "." + targetPart).toLatin1().constData());
        node->SetAttribute("Target", (source->id().toString() + "." + sourcePart).toLatin1().constData());
        node->SetAttribute("Type", "Element");        
        root->LinkEndChild(node);
        auto const &connection = pair.second;
    }


    TiXmlPrinter printer;
    printer.SetIndent("    ");
    document.Accept(&printer);
    std::string xmltext = printer.CStr();
    return QByteArray(xmltext.c_str(), xmltext.size());

    QJsonObject sceneJson;

    QJsonArray nodesJsonArray;

    for (auto const & pair : _nodes)
    {
        auto const &node = pair.second;

        nodesJsonArray.append(node->save());
    }

    sceneJson["nodes"] = nodesJsonArray;

    QJsonArray connectionJsonArray;
    for (auto const & pair : _connections)
    {
        auto const &connection = pair.second;

        QJsonObject connectionJson = connection->save();

        if (!connectionJson.isEmpty())
            connectionJsonArray.append(connectionJson);
    }

    sceneJson["connections"] = connectionJsonArray;

    QJsonDocument documentJson(sceneJson);

    return documentJson.toJson();
}

void FlowXmlScene::loadXmlFromMemory(const QByteArray& data)
{
    std::map<QString, QUuid> remapping;
    TiXmlDocument document;
    document.Parse(data.constData());
    if (strcmp(document.RootElement()->Value(), "Flow") == 0)
    {
        for (TiXmlElement* elm = document.RootElement()->FirstChildElement("Node"); elm != nullptr; elm = elm->NextSiblingElement("Node"))
        {
            QString modelName = elm->Attribute("Type");
            auto dataModel = registry().create(modelName);
            if (!dataModel)
                throw std::logic_error(std::string("No registered model with name ") +
                    modelName.toLocal8Bit().data());

            QUuid id(elm->Attribute("Id"));
            if (id.isNull() == true) id = QUuid::createUuid();
            remapping[elm->Attribute("Id")] = id;
            double x = 0.0, y = 0.0;
            elm->QueryDoubleAttribute("Designer.OffsetX", &x);
            elm->QueryDoubleAttribute("Designer.OffsetY", &y);
            QtNodes::Node& node = createNodeInternal(std::move(dataModel), id);
            node.nodeGraphicsObject().setPos(QPointF(x, y));
            nodePlaced(node);
        }

        for (TiXmlElement* elm = document.RootElement()->FirstChildElement("Connection"); elm != nullptr; elm = elm->NextSiblingElement("Connection"))
        {
            QString Source = QString(elm->Attribute("Source")).split(".").at(0);
            QString Target = QString(elm->Attribute("Target")).split(".").at(0);
            QString SourcePort = QString(elm->Attribute("Source")).split(".").at(1);
            QString TargetPort = QString(elm->Attribute("Target")).split(".").at(1);

            QUuid nodeInId = remapping[Source];
            QUuid nodeOutId = remapping[Target];
            auto nodeIn = _nodes[nodeInId].get();
            auto nodeOut = _nodes[nodeOutId].get();

            DataSource* source = static_cast<DataSource*>(nodeIn->nodeDataModel());
            DataSource* target = static_cast<DataSource*>(nodeOut->nodeDataModel());
            PortIndex portIndexIn = source->find_port(SourcePort, "", PortType::In);
            PortIndex portIndexOut = target->find_port(TargetPort, "", PortType::Out);
            auto getConverter = [&]()
            {                
                return TypeConverter{};
            };

            std::shared_ptr<QtNodes::Connection> connection =
                createConnection(*nodeIn, portIndexIn,
                    *nodeOut, portIndexOut,
                    getConverter());
            //return connection;
        }
    }



        /*
    QJsonObject const jsonDocument = QJsonDocument::fromJson(data).object();

    QJsonArray nodesJsonArray = jsonDocument["nodes"].toArray();

    for (QJsonValueRef node : nodesJsonArray)
    {
        restoreNode(node.toObject());
    }

    QJsonArray connectionJsonArray = jsonDocument["connections"].toArray();

    for (QJsonValueRef connection : connectionJsonArray)
    {
        restoreConnection(connection.toObject());
    }
    */
}


static std::shared_ptr<DataModelRegistry>
registerDataModels()
{

  static TiXmlDocument doc;
  doc.LoadFile("flow.xml");

  auto ret = std::make_shared<DataModelRegistry>();  
  for (TiXmlElement* elm = doc.RootElement()->FirstChildElement("Node"); elm != nullptr; elm = elm->NextSiblingElement("Node"))
  {
      const char* name = elm->Attribute("Name");
      const char* category = elm->Attribute("Category");     
      TiXmlElement* membersA = elm->FirstChildElement("Member");
      ret->registerModel<DataSource>(
          [name, elm]() {
                auto source = std::make_unique<DataSource>(); 
                source->internalName = name; 
                QWidget * central = source->internalWidget = new QWidget();
                central->setLayout(new QVBoxLayout());
                central->setWindowFlags(Qt::FramelessWindowHint);
                central->setAttribute(Qt::WA_NoSystemBackground);
                central->setAttribute(Qt::WA_TranslucentBackground);
                central->setAttribute(Qt::WA_TransparentForMouseEvents);
                for (TiXmlElement* members = elm->FirstChildElement("Member"); members != nullptr; members = members->NextSiblingElement("Member")) {
                    source->internalPorts.push_back(members->Attribute("Name"));
                    source->internalTypes.push_back(members->Attribute("Type"));
                    source->internalDefaults.push_back(members->Attribute("Default"));                    
                    source->internalData.push_back(std::make_shared<DecimalData>());
                    QLineEdit * _lineEdit = new QLineEdit(central);
                    //_lineEdit->setMaximumSize(_lineEdit->sizeHint());
                    _lineEdit->setText(members->Attribute("Default") ? members->Attribute("Default") : "");
                    _lineEdit->setToolTip(members->Attribute("Name"));
                    central->layout()->addWidget(_lineEdit);
                }

                for (TiXmlElement* members = elm->FirstChildElement("Signal"); members != nullptr; members = members->NextSiblingElement("Signal")) 
                {
                    if (strcmp(members->Attribute("Type"), "In") == 0)
                    {
                        source->internalPortsIn.push_back(members->Attribute("Name"));
                        source->internalTypesIn.push_back("Signal");
                        source->internalDefaultsIn.push_back("");
                        source->internalDataIn.push_back(std::make_shared<DecimalData>());
                    }
                    else if (strcmp(members->Attribute("Type"), "Out") == 0)
                    {
                        source->internalPortsOut.push_back(members->Attribute("Name"));
                        source->internalTypesOut.push_back("Signal");
                        source->internalDefaultsOut.push_back("");
                        source->internalDataOut.push_back(std::make_shared<DecimalData>());
                    }
                }

                return source; 
          },
          category
      );
      //< Name = "CycleInt32" Color = "Purple" Category = "Arithmetics">
  }

   
  /*
  ret->registerModel<NumberSourceDataModel>("Sources");

  ret->registerModel<NumberDisplayDataModel>("Displays");

  r et->registerModel<AdditionModel>("Operators");
   
  ret->registerModel<SubtractionModel>("Operators");

  ret->registerModel<MultiplicationModel>("Operators");

  ret->registerModel<DivisionModel>("Operators");

  ret->registerModel<ModuloModel>("Operators");

  ret->registerTypeConverter(std::make_pair(DecimalData().type(),
                                            IntegerData().type()),
                             TypeConverter{DecimalToIntegerConverter()});



  ret->registerTypeConverter(std::make_pair(IntegerData().type(),
                                            DecimalData().type()),
                             TypeConverter{IntegerToDecimalConverter()});

   */
  return ret;
}


static
void
setStyle()
{
  ConnectionStyle::setConnectionStyle(
  R"(
  {
    "ConnectionStyle": {
      "ConstructionColor": "gray",
      "NormalColor": "black",
      "SelectedColor": "gray",
      "SelectedHaloColor": "deepskyblue",
      "HoveredColor": "deepskyblue",

      "LineWidth": 3.0,
      "ConstructionLineWidth": 2.0,
      "PointDiameter": 10.0,

      "UseDataDefinedColors": true
    }
  }
  )");
}


int
main(int argc, char *argv[])
{
  QApplication app(argc, argv);

  setStyle();

  QWidget mainWidget;

  auto menuBar    = new QMenuBar();
  auto slider = new QSlider();
  auto saveAction = menuBar->addAction("Save..");
  auto loadAction = menuBar->addAction("Load..");
  auto saveActionXml = menuBar->addAction("Save xml..");
  auto loadActionXml = menuBar->addAction("Load xml..");  
  auto toolBar = new QToolBar();
  slider->setOrientation(Qt::Orientation::Horizontal);
  slider->setRange(1, 100);
  slider->setValue(100);
  toolBar->addWidget(slider);

  QVBoxLayout *l = new QVBoxLayout(&mainWidget);

  l->addWidget(menuBar);
  l->addWidget(toolBar);
  auto scene = new FlowXmlScene(registerDataModels(), &mainWidget);
  auto view = new FlowView(scene);
  l->addWidget(view);
  l->setContentsMargins(0, 0, 0, 0);
  l->setSpacing(0);

  QObject::connect(saveAction, &QAction::triggered,
                   scene, &FlowScene::save);

  QObject::connect(loadAction, &QAction::triggered,
                   scene, &FlowScene::load);

  QObject::connect(slider, &QSlider::valueChanged,
      view, &FlowView::scaleUniform);

  QObject::connect(saveActionXml, &QAction::triggered,
      scene, &FlowXmlScene::saveXml);

  QObject::connect(loadActionXml, &QAction::triggered,
      scene, &FlowXmlScene::loadXml);

  mainWidget.setWindowTitle("Dataflow tools: simplest calculator");
  mainWidget.resize(800, 600);
  mainWidget.showNormal();

  return app.exec();
}
