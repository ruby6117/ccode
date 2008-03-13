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


void OdorBank::init()
{
    setMaxPWM(100.0);
    setMinPWM(0.0);
    setMaxFlow(100.0);
    setMinFlow(0.0);
    setOdorMap(OdorMap());
}

void OdorBank::destroy()
{

}

void OdorBank::setID( int id )
{
    bank_id = id;
    redrawBankNameTexts();
}


void OdorBank::redrawBankNameTexts()
{
    flow_GB->setTitle(QString("Bank %1 Flow").arg(bank_id));
    enable_Chk->setText(QString("Bank %1 %2").arg(bank_id).arg( enable_Chk->isChecked() ? "ON" : "OFF"));
    pwm_GB->setTitle(QString("Bank %1 PWM").arg(bank_id));
}


void OdorBank::setActualFlow( double flow )
{
    actualFlow_Txt->setText(QString("%1 ml/m").arg(flow, 5, 'f', 4));
}

void OdorBank::setEnabled( bool b)
{
    enable_Chk->setChecked(b);	
    redrawBankNameTexts();
}


void OdorBank::setOdorMap( const OdorMap & in)
{
    OdorMap m(in);
    blockSignals(true);
    int saved_item = odor_CB->currentItem();
    odor_CB->clear();
    int i = 0;
    if (!m.contains(0)) m[0] = "No Odor";
    for (OdorMap::const_iterator it = m.begin(); it != m.end(); ++it, ++i) {
	odor_CB->insertItem(QString("%1 - %2").arg(i).arg(it.data()));
    }
   if (saved_item > -1 && saved_item < odor_CB->count()) odor_CB->setCurrentItem(saved_item);
    blockSignals(false);
}

void OdorBank::setActualPWM( double pct )
{
    actualpwm_txt->setText(QString("%1%%").arg(pct, 3, 'f', 0));
}


void OdorBank::fwdPWMChange()
{
    double d;
    bool ok = false;
    
    d = pwm_le->text().toDouble(&ok);
    if (!ok || d < min_pwm || d > max_pwm) return;
    emit pwmChanged(d);
}


void OdorBank::fwdFlowChange()
{
    double d;
    bool ok = false;
    
    d = flow_le->text().toDouble(&ok);
    if (!ok || d < min_flow || d > max_flow) return;
    emit flowChanged(d);
}


void OdorBank::setCommandedFlow( double  d)
{
    if (d < min_flow || d > max_flow) return;
    flow_le->setText(QString("%1").arg(d, 4, 'f', 1));
}

void OdorBank::setCommandedPWM( double d )
{
    if (d >= min_pwm && d <= max_pwm)
	pwm_le->setText(QString("%1%%").arg(d, 3, 'f', 0));
}


void OdorBank::fwdEnableChange()
{
    OdorBank::setEnabled(enable_Chk->isChecked());
    emit bankEnableSet(enable_Chk->isChecked());
}

void OdorBank::fwdOdorChanged()
{
    if ( odor_CB->currentItem() > -1 )
	emit odorChanged(odor_CB->currentItem());
}


void OdorBank::setMaxFlow( double d)
{
    max_flow = d;
}


void OdorBank::setMinFlow( double d)
{
    min_flow = d;
}


void OdorBank::setMaxPWM( double d)
{
    max_pwm = d;
}


void OdorBank::setMinPWM( double d)
{
    min_pwm = d;
}

double OdorBank::maxFlow() const
{
    return max_flow;
}

double OdorBank::minFlow()  const
{
    return min_flow;
}

double OdorBank::maxPWM() const
{
    return max_pwm;
}


double OdorBank::minPWM() const
{
    return min_pwm;
}


OdorMap OdorBank::odorMap() const
{
    OdorMap m;
    for (int i = 0; i < odor_CB->count(); ++i)
	m[i] = odor_CB->text(i);
    return m;
}


int OdorBank::id() const
{
    return bank_id;
}

