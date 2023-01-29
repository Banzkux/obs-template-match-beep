#include "CustomBeepSettings.h"
#include "template-match-beep.generated.h"

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

// Create setting row with loaded settings
ArrayItemWidget::ArrayItemWidget(obs_data_t *item, CustomBeepSettings *settings, QWidget *parent)
	: ArrayItemWidget(settings, parent)
{
	type->setCurrentIndex(obs_data_get_int(item, SETTING_EVENT_TYPE));
	length->setValue(obs_data_get_int(item, SETTING_EVENT_LENGTH));
	frequency->setValue(obs_data_get_int(item, SETTING_EVENT_FREQUENCY));
}

ArrayItemWidget::ArrayItemWidget(CustomBeepSettings *settings, QWidget *parent)
	: QWidget(parent), settings(settings)
{
	layout = new QHBoxLayout(this);
	QMargins margins(0, 0, 0, 0);
	layout->setContentsMargins(margins);

	type = new QComboBox(this);

	type->addItem("Beep", (int)EventType::Beep);
	type->addItem("Wait", (int)EventType::Wait);
	type->setMaximumWidth(65);
	type->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
	type->installEventFilter(new MouseWheelWidgetAdjustmentGuard(type));

	connect(type, SIGNAL(currentIndexChanged(int)), this, SLOT(currentTypeChanged(int)));

	length = new QSpinBox(this);
	length->setMinimum(1);
	length->setMaximum(INT_MAX);

	length->setSuffix(" ms");
	length->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
	length->installEventFilter(new MouseWheelWidgetAdjustmentGuard(length));
	length->setValue(100);

	connect(length, SIGNAL(valueChanged(int)), this, SLOT(currentLengthChanged(int)));

	frequency = new QSpinBox(this);
	frequency->setMinimum(37);
	frequency->setMaximum(32767);
	frequency->setSuffix(" Hz");
	frequency->setVisible(type->currentIndex() == (int)EventType::Beep);
	frequency->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
	frequency->installEventFilter(new MouseWheelWidgetAdjustmentGuard(frequency));
	frequency->setValue(440);

	connect(frequency, SIGNAL(valueChanged(int)), this, SLOT(currentFrequencyChanged(int)));

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

void ArrayItemWidget::currentTypeChanged(int index)
{
	settings->ChangedArrayItem(this, SETTING_EVENT_TYPE, index);
	if (index == (int)EventType::Beep) {
		frequency->setVisible(true);
	} else {
		frequency->setVisible(false);
	}
}

void ArrayItemWidget::currentLengthChanged(int value) {
	settings->ChangedArrayItem(this, SETTING_EVENT_LENGTH, value);
}

void ArrayItemWidget::currentFrequencyChanged(int value)
{
	settings->ChangedArrayItem(this, SETTING_EVENT_FREQUENCY, value);
}

void ArrayItemWidget::removeClicked()
{
	settings->DeleteArrayItem(this);
	delete this;
}

CustomBeepSettings::CustomBeepSettings(QObject *parent) : QObject(parent), m_Settings(nullptr)
{
	window = new QDialog();
	window->resize(420, 496);

	window->setWindowTitle(QApplication::translate("beep_settings", "Beep Settings"));
	mainLayout = new QVBoxLayout(window);

	scrollarea = new QScrollArea(window);
	mainLayout->addWidget(scrollarea);
	scrollarea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scrollarea->setWidgetResizable(true);
	scrollarea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	techArea = new QWidget(window);

	techArea->setObjectName("techarea");
	techArea->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	list = new QVBoxLayout(techArea);
	list->setAlignment(Qt::AlignTop);

	techArea->setLayout(list);
	scrollarea->setWidget(techArea);

	button = new QPushButton("Add event", window);
	connect(button, &QPushButton::clicked, this, &CustomBeepSettings::addNewEvent);
	mainLayout->addWidget(button);
}

CustomBeepSettings::~CustomBeepSettings() {}

void CustomBeepSettings::CreateOBSSettings(obs_data_t *settings)
{
	m_Settings = obs_data_get_array(settings, SETTING_EVENT_ARRAY);
	if (m_Settings == nullptr) {
		m_Settings = obs_data_array_create();
		obs_data_set_array(settings, SETTING_EVENT_ARRAY, m_Settings);
	} else {
		// Create UI from loaded settings
		for (int i = 0; i < obs_data_array_count(m_Settings); i++) {
			obs_data_t *item = obs_data_array_item(m_Settings, i);
			list->addWidget(new ArrayItemWidget(item, this, window));
		}
	}
}

void CustomBeepSettings::CreateSettingsWindow()
{
	window->show();
	window->exec();
}

void CustomBeepSettings::DeleteArrayItem(ArrayItemWidget *widget)
{
	obs_data_array_erase(m_Settings, list->indexOf(widget));
}

void CustomBeepSettings::ChangedArrayItem(ArrayItemWidget* widget, const char* name, int value) {
	int index = list->indexOf(widget);
	obs_data_t *item = obs_data_array_item(m_Settings, index);
	obs_data_set_int(item, name, value);
}

std::vector<Event> CustomBeepSettings::GetEvents() {
	std::vector<Event> events;
	
	for (int i = 0; i < obs_data_array_count(m_Settings); i++) {
		Event current;
		obs_data_t *item = obs_data_array_item(m_Settings, i);
		current.type = static_cast<EventType>(obs_data_get_int(item, SETTING_EVENT_TYPE));
		current.length = obs_data_get_int(item, SETTING_EVENT_LENGTH);
		current.frequency = obs_data_get_int(item, SETTING_EVENT_FREQUENCY);
		events.push_back(current);
	}

	return events;
}

void CustomBeepSettings::addNewEvent()
{
	obs_data_array_push_back(m_Settings, CreateArrayItem(EventType::Beep));
	list->addWidget(new ArrayItemWidget(this, window));
}

obs_data_t *CustomBeepSettings::CreateArrayItem(EventType type)
{
	obs_data_t *item = obs_data_create();
	obs_data_set_int(item, SETTING_EVENT_TYPE, (int)type);

	obs_data_set_int(item, SETTING_EVENT_LENGTH, 100);

	obs_data_set_int(item, SETTING_EVENT_FREQUENCY, 440);

	return item;
}

void CustomBeepSettings::SetArrayItemType(obs_data_t *item, EventType type)
{
	EventType curType = static_cast<EventType>(obs_data_get_int(item, SETTING_EVENT_TYPE));

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