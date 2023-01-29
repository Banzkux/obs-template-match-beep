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
	explicit MouseWheelWidgetAdjustmentGuard(QObject *parent);

protected:
	bool eventFilter(QObject *o, QEvent *e) override;
};

enum class EventType { None, Beep, Wait };

class ArrayItemWidget : public QWidget {
	Q_OBJECT
public:
	ArrayItemWidget(QWidget *parent = nullptr);
	~ArrayItemWidget();

private slots:
	void currentIndexChangedC(int index);
	void removeClicked();

private:
	QHBoxLayout *layout;
	QComboBox *type;
	QSpinBox *length;
	QSpinBox *frequency;
	QPushButton *button;
};

class CustomBeepSettings : public QObject {
	Q_OBJECT
public:
	CustomBeepSettings(QObject *parent = nullptr);
	~CustomBeepSettings();

    void CreateOBSSettings(obs_data_t *settings);

	void CreateSettingsWindow();

private:
	void addNewEvent();

    obs_data_t *CreateArrayItem(EventType type);

	void SetArrayItemType(obs_data_t *item, EventType type);

	QDialog *window;
	QVBoxLayout *mainLayout;
	QScrollArea *scrollarea;
	QWidget *techArea;
	QVBoxLayout *list;
	QPushButton *button;
};
#endif // !CUSTOMBEEPSETTINGS_H