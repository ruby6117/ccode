/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/


void CarrierBank::init()
{
    setID(0);
    setMaxFlow(1000.0);
    setMinFlow(900.0);
}

void CarrierBank::fwdFlowChanged()
{
    double d;
    bool ok = false;
    
    d = flow_le->text().toDouble(&ok);
    if (!ok || d < min_flow || d > max_flow) return;
    emit flowChanged(d);
}

void CarrierBank::setActualFlow( double d )
{
    flow_txt->setText(QString("%1 ml/m").arg(d, 4, 'f', 1));
}



void CarrierBank::setMinFlow( double d )
{
    min_flow = d;
}


void CarrierBank::setMaxFlow( double d )
{
    max_flow = d;
}


double CarrierBank::minFlow() const
{
    return min_flow;
}



double CarrierBank::maxFlow()  const
{
    return max_flow;
}


void CarrierBank::setID(int id)
{
    carrier_GB->setTitle(QString("Carrier %1 Air").arg(id));
    carrier_id = id;
}



int CarrierBank::id() const
{
    return carrier_id;
}
