// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Copyright (c) 2014-2021 The Dash Core Developers
// Copyright (c) 2009-2021 The Bitcoin Developers
// Copyright (c) 2009-2021 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_QT_TRAFFICGRAPHWIDGET_H
#define CASH_QT_TRAFFICGRAPHWIDGET_H

#include "trafficgraphdata.h"

#include <boost/function.hpp>

#include <QQueue>
#include <QWidget>

class ClientModel;

QT_BEGIN_NAMESPACE
class QPaintEvent;
class QTimer;
QT_END_NAMESPACE

class TrafficGraphWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TrafficGraphWidget(QWidget* parent = 0);
    void setClientModel(ClientModel* model);
    int getGraphRangeMins() const;

protected:
    void paintEvent(QPaintEvent*);

public Q_SLOTS:
    void updateRates();
    void setGraphRangeMins(int value);
    void clear();

private:
    typedef boost::function<float(const TrafficSample&)> SampleChooser;
    void paintPath(QPainterPath& path, const TrafficGraphData::SampleQueue& queue, SampleChooser chooser);

    QTimer* timer;
    float fMax;
    int nMins;
    ClientModel* clientModel;
    TrafficGraphData trafficGraphData;
};

#endif // CASH_QT_TRAFFICGRAPHWIDGET_H