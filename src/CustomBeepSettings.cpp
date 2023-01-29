#include "CustomBeepSettings.h"

MouseWheelWidgetAdjustmentGuard::MouseWheelWidgetAdjustmentGuard(QObject *parent)
	: QObject(parent)
{
}

bool MouseWheelWidgetAdjustmentGuard::eventFilter(QObject *o, QEvent *e)
{
	const QWidget *widget = static_cast<QWidget *>(o);
	if (e->type() == QEvent::Wheel && widget && !widget->hasFocus()) {
		e->ignore();
		return true;
	}

	return QObject::eventFilter(o, e);
}

ArrayItemWidget::ArrayItemWidget(QWidget *parent) : QWidget(parent)
{
	layout = new QHBoxLayout(this);

	type = new QComboBox(this);

	type->addItem("Beep", (int)EventType::Beep);
	type->addItem("Wait", (int)EventType::Wait);
	type->setMaximumWidth(65);
	type->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
	type->installEventFilter(new MouseWheelWidgetAdjustmentGuard(type));

	connect(type, SIGNAL(currentIndexChanged(int)), this, SLOT(currentIndexChangedC(int)));

	length = new QSpinBox(this);
	length->setMinimum(1);
	length->setMaximum(INT_MAX);

	length->setSuffix(" ms");
	length->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
	length->installEventFilter(new MouseWheelWidgetAdjustmentGuard(length));

	frequency = new QSpinBox(this);
	frequency->setMinimum(37);
	frequency->setMaximum(32767);
	frequency->setSuffix(" Hz");
	frequency->setVisible(type->currentIndex() + 1 == (int)EventType::Beep);
	frequency->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
	frequency->installEventFilter(new MouseWheelWidgetAdjustmentGuard(frequency));

	button = new QPushButton(this);
	button->setText("x");
	button->setMaximumWidth(38);

	connect(button, SIGNAL(clicked()), this, SLOT(removeClicked()));

	layout->addWidget(type);
	layout->addWidget(length);
	layout->addWidget(frequency);
	layout->addWidget(button);
}

ArrayItemWidget::~ArrayItemWidget()
{
	delete layout;
}

void ArrayItemWidget::currentIndexChangedC(int index)
{
	blog(LOG_INFO, "HEYY %i", index);
	if (index + 1 == (int)EventType::Beep) {
		frequency->setVisible(true);
	} else {
		frequency->setVisible(false);
	}
}

void ArrayItemWidget::removeClicked()
{
	blog(LOG_INFO, "REMOVE CLICKED!");
}

CustomBeepSettings::CustomBeepSettings(QObject *parent) : QObject(parent)
{
	window = new QDialog();
	window->resize(420, 496);

	window->setWindowTitle(QApplication::translate("beep_settings", "Beep Settings"));
	mainLayout = new QVBoxLayout(window);

	scrollarea = new QScrollArea(window);
	mainLayout->addWidget(scrollarea);
	scrollarea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scrollarea->setWidgetResizable(true);

	techArea = new QWidget(window);

	techArea->setObjectName("techarea");
	techArea->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	list = new QVBoxLayout(techArea);

	techArea->setLayout(list);
	scrollarea->setWidget(techArea);

	button = new QPushButton("Add event", window);
	connect(button, &QPushButton::clicked, this, &CustomBeepSettings::addNewEvent);
	mainLayout->addWidget(button);
}

CustomBeepSettings::~CustomBeepSettings() {}

void CustomBeepSettings::CreateOBSSettings(obs_data_t *settings)
{
	obs_data_array_t *dataarray = obs_data_get_array(settings, SETTING_EVENT_ARRAY);
	if (dataarray == nullptr) {
		blog(LOG_INFO, "Array doesn't exist!");
		dataarray = obs_data_array_create();
		obs_data_set_array(settings, SETTING_EVENT_ARRAY, dataarray);
	}
	blog(LOG_INFO, "Array count: %i", obs_data_array_count(dataarray));
	obs_data_t *arrayitem = obs_data_create();
	obs_data_array_push_back(dataarray, arrayitem);
	blog(LOG_INFO, "Array count: %i", obs_data_array_count(dataarray));
}

void CustomBeepSettings::CreateSettingsWindow()
{
	window->show();
	blog(LOG_INFO, "%ix%i", window->size().width(), window->size().height());
	window->exec();
}

void CustomBeepSettings::addNewEvent()
{
	blog(LOG_INFO, "TEST EVENT");
	list->addWidget(new ArrayItemWidget(window));
}

obs_data_t *CustomBeepSettings::CreateArrayItem(EventType type)
{
	obs_data_t *item = obs_data_create();
	obs_data_set_int(item, SETTING_EVENT_TYPE, (int)type);

	obs_data_set_int(item, SETTING_EVENT_LENGTH, 100);

	if (type == EventType::Beep)
		obs_data_set_int(item, SETTING_EVENT_FREQUENCY, 440);

	return item;
}

void CustomBeepSettings::SetArrayItemType(obs_data_t *item, EventType type)
{
	EventType curType = (EventType)obs_data_get_int(item, SETTING_EVENT_TYPE);

	// Ignore if type didn't change
	if (curType == type)
		return;

	// Clear previous data
	obs_data_clear(item);

	// Create needed data, depending on the type
	obs_data_set_int(item, SETTING_EVENT_TYPE, (int)type);
	// All events have length
	obs_data_set_int(item, SETTING_EVENT_LENGTH, 100);
	// Beep sound frequency
	if (type == EventType::Beep) {
		obs_data_set_int(item, SETTING_EVENT_FREQUENCY, 440);
	}
}