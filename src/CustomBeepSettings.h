/*
OBS Template Match Beep
Copyright (C) 2022 Janne Pitkanen <acebanzkux@gmail.com>

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

#ifndef CUSTOMBEEPSETTINGS_H
#define CUSTOMBEEPSETTINGS_H

#include <util/base.h>
#include <obs-data.h>
#include <QtWidgets>

#define SETTING_EVENT_ARRAY "event_array"

#define SETTING_EVENT_TYPE "event_type"
#define SETTING_EVENT_LENGTH "event_length"
#define SETTING_EVENT_FREQUENCY "event_frequency"

class MouseWheelWidgetAdjustmentGuard : public QObject {
public:
	explicit MouseWheelWidgetAdjustmentGuard(QObject *parent)
		: QObject(parent){}

protected:
	bool eventFilter(QObject* o, QEvent* e) override {
		const QWidget *widget = static_cast<QWidget *>(o);
		if (e->type() == QEvent::Wheel && widget &&
		    !widget->hasFocus()) {
			e->ignore();
			return true;
		}

		return QObject::eventFilter(o, e);
	}
};

enum class EventType { None, Beep, Wait };

class ArrayItemWidget : public QWidget {
	Q_OBJECT

private slots:
	void currentIndexChangedC(int index)
	{
		blog(LOG_INFO, "HEYY %i", index);
		if (index + 1 == (int)EventType::Beep) {
			frequency->setVisible(true);
		} else {
			frequency->setVisible(false);
		}
	}

	void removeClicked() { 
		blog(LOG_INFO, "REMOVE CLICKED!");
	}

public:
	ArrayItemWidget(QWidget *parent = nullptr) : QWidget(parent)
	{
		layout = new QHBoxLayout(this);

		type = new QComboBox(this);

		type->addItem("Beep", (int)EventType::Beep);
		type->addItem("Wait", (int)EventType::Wait);
		type->setMaximumWidth(65);
		type->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
		type->installEventFilter(
			new MouseWheelWidgetAdjustmentGuard(type));

		connect(type, SIGNAL(currentIndexChanged(int)), this, SLOT(currentIndexChangedC(int)));

		length = new QSpinBox(this);
		length->setMinimum(1);
		length->setMaximum(INT_MAX);

		length->setSuffix(" ms");
		length->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
		length->installEventFilter(
			new MouseWheelWidgetAdjustmentGuard(length));

		frequency = new QSpinBox(this);
		frequency->setMinimum(37);
		frequency->setMaximum(32767);
		frequency->setSuffix(" Hz");
		frequency->setVisible(type->currentIndex() + 1 ==
				     (int)EventType::Beep);
		frequency->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
		frequency->installEventFilter(
			new MouseWheelWidgetAdjustmentGuard(frequency));

		button = new QPushButton(this);
		button->setText("x");
		button->setMaximumWidth(38);

		connect(button, SIGNAL(clicked()), this,
			SLOT(removeClicked()));

		
		layout->addWidget(type);
		layout->addWidget(length);
		layout->addWidget(frequency);
		layout->addWidget(button);
	}

	~ArrayItemWidget() { delete layout; }

	/*QSize sizeHint() const override { 
		return QSize(200, 200);
	}*/


private:
	QHBoxLayout *layout;
	QComboBox *type;
	QSpinBox *length;
	QSpinBox *frequency;
	QPushButton *button;
};

class CustomBeepSettings : public QObject {
	Q_OBJECT

	QDialog *window;
	QVBoxLayout *mainLayout;
	QScrollArea *scrollarea;
	QWidget *techArea;
	QVBoxLayout *list;
	QPushButton *button;

public:
	CustomBeepSettings(QObject* parent = nullptr) : QObject(parent) {
		window = new QDialog();
		window->resize(420, 496);

		window->setWindowTitle(
			QApplication::translate("beep_settings", "Beep Settings"));
		mainLayout = new QVBoxLayout(window);

		scrollarea = new QScrollArea();
		mainLayout->addWidget(scrollarea);
		scrollarea->setHorizontalScrollBarPolicy(
			Qt::ScrollBarAlwaysOff);
		scrollarea->setWidgetResizable(true);

		techArea = new QWidget();

		techArea->setObjectName("techarea");
		techArea->setSizePolicy(QSizePolicy::MinimumExpanding,
					QSizePolicy::MinimumExpanding);
		list = new QVBoxLayout(techArea);

		techArea->setLayout(list);
		scrollarea->setWidget(techArea);

		button = new QPushButton("Add event");
		connect(button, &QPushButton::clicked, this,
			&CustomBeepSettings::addNewEvent);
		mainLayout->addWidget(button);
	}

	~CustomBeepSettings() {}

    void CreateOBSSettings(obs_data_t *settings) {
	    obs_data_array_t *dataarray =
		    obs_data_get_array(settings, SETTING_EVENT_ARRAY);
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

	void CreateSettingsWindow()
	{
	    window->show();
	    blog(LOG_INFO, "%ix%i", window->size().width(),
		 window->size().height());
	    window->exec();
	}

private:
	void addNewEvent() { 
		blog(LOG_INFO, "TEST EVENT");
	    QComboBox *q = new QComboBox();
		q->addItem("Beep", (int)EventType::Beep);
	    q->addItem("Wait", (int)EventType::Wait);
	    list->addWidget(new ArrayItemWidget());
	}

    obs_data_t* CreateArrayItem(EventType type) { 
		obs_data_t *item = obs_data_create();
	    obs_data_set_int(item, SETTING_EVENT_TYPE, (int)type);

		obs_data_set_int(item, SETTING_EVENT_LENGTH, 100);

		if (type == EventType::Beep)
		    obs_data_set_int(item, SETTING_EVENT_FREQUENCY, 440);

		return item;
	}

	void SetArrayItemType(obs_data_t* item, EventType type) {
		EventType curType =
			(EventType)obs_data_get_int(item, SETTING_EVENT_TYPE);

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
};
#endif // !CUSTOMBEEPSETTINGS_H
