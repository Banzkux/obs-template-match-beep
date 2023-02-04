/*
OBS Template Match Beep
Copyright (C) 2022 - 2023 Janne Pitkänen <acebanzkux@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "CustomBeepSettings.h"
#include "template-match-beep.generated.h"

MouseWheelWidgetAdjustmentGuard::MouseWheelWidgetAdjustmentGuard(QObject *parent) : QObject(parent)
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
	m_Type->setCurrentIndex(obs_data_get_int(item, SETTING_EVENT_TYPE));
	m_Length->setValue(obs_data_get_int(item, SETTING_EVENT_LENGTH));
	m_Frequency->setValue(obs_data_get_int(item, SETTING_EVENT_FREQUENCY));
}

ArrayItemWidget::ArrayItemWidget(CustomBeepSettings *settings, QWidget *parent)
	: QWidget(parent), m_Settings(settings)
{
	m_Layout = new QHBoxLayout(this);
	QMargins margins(0, 0, 0, 0);
	m_Layout->setContentsMargins(margins);

	m_Type = new QComboBox(this);

	m_Type->addItem("Beep", (int)EventType::Beep);
	m_Type->addItem("Wait", (int)EventType::Wait);
	m_Type->setMaximumWidth(65);
	m_Type->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
	m_Type->installEventFilter(new MouseWheelWidgetAdjustmentGuard(m_Type));

	connect(m_Type, SIGNAL(currentIndexChanged(int)), this, SLOT(currentTypeChanged(int)));

	m_Length = new QSpinBox(this);
	m_Length->setMinimum(1);
	m_Length->setMaximum(INT_MAX);

	m_Length->setSuffix(" ms");
	m_Length->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
	m_Length->installEventFilter(new MouseWheelWidgetAdjustmentGuard(m_Length));
	m_Length->setValue(100);

	connect(m_Length, SIGNAL(valueChanged(int)), this, SLOT(currentLengthChanged(int)));

	m_Frequency = new QSpinBox(this);
	m_Frequency->setMinimum(37);
	m_Frequency->setMaximum(32767);
	m_Frequency->setSuffix(" Hz");
	m_Frequency->setVisible(m_Type->currentIndex() == (int)EventType::Beep);
	m_Frequency->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
	m_Frequency->installEventFilter(new MouseWheelWidgetAdjustmentGuard(m_Frequency));
	m_Frequency->setValue(440);

	connect(m_Frequency, SIGNAL(valueChanged(int)), this, SLOT(currentFrequencyChanged(int)));

	m_Button = new QPushButton(this);
	m_Button->setProperty("themeID", "removeIconSmall");
	m_Button->setMaximumWidth(38);

	connect(m_Button, SIGNAL(clicked()), this, SLOT(removeClicked()));

	m_Layout->addWidget(m_Type);
	m_Layout->addWidget(m_Length);
	m_Layout->addWidget(m_Frequency);
	m_Layout->addWidget(m_Button);
}

ArrayItemWidget::~ArrayItemWidget()
{
	delete m_Layout;
}

void ArrayItemWidget::currentTypeChanged(int index)
{
	m_Settings->ChangedArrayItem(this, SETTING_EVENT_TYPE, index);
	if (index == (int)EventType::Beep) {
		m_Frequency->setVisible(true);
	} else {
		m_Frequency->setVisible(false);
	}
}

void ArrayItemWidget::currentLengthChanged(int value)
{
	m_Settings->ChangedArrayItem(this, SETTING_EVENT_LENGTH, value);
}

void ArrayItemWidget::currentFrequencyChanged(int value)
{
	m_Settings->ChangedArrayItem(this, SETTING_EVENT_FREQUENCY, value);
}

void ArrayItemWidget::removeClicked()
{
	m_Settings->DeleteArrayItem(this);
	delete this;
}

CustomBeepSettings::CustomBeepSettings(QObject *parent)
	: QObject(parent),
	  m_Settings(nullptr),
	  m_Button(nullptr),
	  m_List(nullptr),
	  m_MainLayout(nullptr),
	  m_ScrollArea(nullptr),
	  m_TechArea(nullptr),
	  m_Window(nullptr)
{
}

CustomBeepSettings::~CustomBeepSettings() {}

void CustomBeepSettings::CreateOBSSettings(obs_data_t *settings)
{
	m_Settings = obs_data_get_array(settings, SETTING_EVENT_ARRAY);
	if (m_Settings == nullptr) {
		m_Settings = obs_data_array_create();
		obs_data_set_array(settings, SETTING_EVENT_ARRAY, m_Settings);
	}
}

void CustomBeepSettings::LoadSettings()
{
	// Create UI from loaded settings
	for (int i = 0; i < obs_data_array_count(m_Settings); i++) {
		obs_data_t *item = obs_data_array_item(m_Settings, i);
		m_List->addWidget(new ArrayItemWidget(item, this, m_Window));
	}
}

void CustomBeepSettings::CreateSettingsWindow()
{
	if (m_Window != nullptr)
		return;

	m_Window = new QDialog();
	m_Window->resize(420, 496);
	m_Window->setWindowTitle(QApplication::translate("beep_settings", "Beep Settings"));
	connect(m_Window, SIGNAL(finished(int)), this, SLOT(WindowClosed(int)));

	m_MainLayout = new QVBoxLayout(m_Window);

	m_ScrollArea = new QScrollArea(m_Window);
	m_MainLayout->addWidget(m_ScrollArea);
	m_ScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_ScrollArea->setWidgetResizable(true);
	m_ScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	m_TechArea = new QWidget(m_Window);

	m_TechArea->setObjectName("techarea");
	m_TechArea->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	m_List = new QVBoxLayout(m_TechArea);
	m_List->setAlignment(Qt::AlignTop);

	m_TechArea->setLayout(m_List);
	m_ScrollArea->setWidget(m_TechArea);

	m_Button = new QPushButton("Event", m_Window);
	m_Button->setProperty("themeID", "addIconSmall");
	connect(m_Button, &QPushButton::clicked, this, &CustomBeepSettings::addNewEvent);
	m_MainLayout->addWidget(m_Button);

	LoadSettings();
}

void CustomBeepSettings::ShowSettingsWindow()
{
	CreateSettingsWindow();

	m_Window->show();
	m_Window->activateWindow();
}

void CustomBeepSettings::DeleteArrayItem(ArrayItemWidget *widget)
{
	obs_data_array_erase(m_Settings, m_List->indexOf(widget));
}

void CustomBeepSettings::ChangedArrayItem(ArrayItemWidget *widget, const char *name, int value)
{
	int index = m_List->indexOf(widget);
	obs_data_t *item = obs_data_array_item(m_Settings, index);
	obs_data_set_int(item, name, value);
}

std::vector<Event> CustomBeepSettings::GetEvents()
{
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

void CustomBeepSettings::WindowClosed(int result)
{
	delete m_Window;
	m_Window = nullptr;
	UNUSED_PARAMETER(result);
}

void CustomBeepSettings::addNewEvent()
{
	obs_data_array_push_back(m_Settings, CreateArrayItem(EventType::Beep));
	m_List->addWidget(new ArrayItemWidget(this, m_Window));
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