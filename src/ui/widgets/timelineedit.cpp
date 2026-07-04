/***** BEGIN LICENSE BLOCK *****

BSD License - see timeline.h

***** END LICENCE BLOCK *****/

#include "timeline.h"
#include "timeline_p.h"

#include "model/nifmodel.h"
#include "model/undocommands.h"
#include "data/nifitem.h"
#include "nifsnapshot.h"

#include <QBuffer>
#include <QFileInfo>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTextStream>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

//! @file timelineedit.cpp TimelineWidget editing: key IO, undo, clipboard, CSV, lint, settings

/*
 *  Low level field access
 */

void TimelineWidget::pushFieldChange( const QModelIndex & fieldIdx, const QVariant & v, bool newTransaction )
{
	if ( !nif || !fieldIdx.isValid() )
		return;

	QModelIndex vIdx = fieldIdx.sibling( fieldIdx.row(), NifModel::ValueCol );
	const NifItem * item = static_cast<const NifItem *>( vIdx.internalPointer() );
	if ( !item )
		return;

	NifValue oldVal = item->value();
	NifValue newVal = oldVal;

	bool ok = false;
	switch ( (int)v.userType() ) {
	case QMetaType::Float:
	case QMetaType::Double:
		ok = newVal.set<float>( (float)v.toDouble(), nif, item );
		break;
	case QMetaType::Int:
	case QMetaType::UInt:
	case QMetaType::LongLong:
		ok = newVal.setFromString( QString::number( v.toLongLong() ), nif, item );
		break;
	case QMetaType::QString:
		ok = newVal.setFromString( v.toString(), nif, item );
		break;
	default:
		ok = newVal.setFromVariant( v );
		break;
	}

	if ( !ok )
		return;

	if ( newTransaction )
		ChangeValueCommand::createTransaction();

	nif->undoStack->push( new ChangeValueCommand( vIdx, oldVal, newVal, nif->itemName( fieldIdx ), nif ) );
}

template <typename T>
static void tlPushTypedChange( NifModel * nif, const QModelIndex & fieldIdx, const T & v )
{
	if ( !nif || !fieldIdx.isValid() )
		return;

	QModelIndex vIdx = fieldIdx.sibling( fieldIdx.row(), NifModel::ValueCol );
	const NifItem * item = static_cast<const NifItem *>( vIdx.internalPointer() );
	if ( !item )
		return;

	NifValue oldVal = item->value();
	NifValue newVal = oldVal;
	if ( !newVal.set<T>( v, nif, item ) )
		return;

	nif->undoStack->push( new ChangeValueCommand( vIdx, oldVal, newVal, nif->itemName( fieldIdx ), nif ) );
}

float TimelineWidget::keyComponentValue( const TimelineChannel & ch, const QModelIndex & keyIdx, int comp ) const
{
	if ( !nif || !keyIdx.isValid() )
		return 0;

	switch ( ch.type ) {
	case TimelineChannel::Float:
		return nif->get<float>( keyIdx, "Value" );
	case TimelineChannel::Vector3Val:
		return nif->get<Vector3>( keyIdx, "Value" )[std::clamp( comp, 0, 2 )];
	case TimelineChannel::Color3Val:
		{
			Color3 c = nif->get<Color3>( keyIdx, "Value" );
			return comp == 0 ? c.red() : comp == 1 ? c.green() : c.blue();
		}
	case TimelineChannel::Color4Val:
		{
			Color4 c = nif->get<Color4>( keyIdx, "Value" );
			return comp == 0 ? c.red() : comp == 1 ? c.green() : comp == 2 ? c.blue() : c.alpha();
		}
	case TimelineChannel::BoolVal:
		return nif->get<int>( keyIdx, "Value" ) ? 1.0f : 0.0f;
	case TimelineChannel::QuatVal:
		return nif->get<Quat>( keyIdx, "Value" )[std::clamp( comp, 0, 3 )];
	default:
		return 0;
	}
}

void TimelineWidget::setKeyComponentValue( const TimelineChannel & ch, const QModelIndex & keyIdx, int comp, float v )
{
	if ( !nif || !keyIdx.isValid() )
		return;

	QModelIndex iVal = nif->getIndex( keyIdx, "Value" );
	if ( !iVal.isValid() )
		return;

	switch ( ch.type ) {
	case TimelineChannel::Float:
		tlPushTypedChange<float>( nif, iVal, v );
		break;
	case TimelineChannel::Vector3Val:
		{
			Vector3 vec = nif->get<Vector3>( keyIdx, "Value" );
			vec[std::clamp( comp, 0, 2 )] = v;
			tlPushTypedChange<Vector3>( nif, iVal, vec );
		}
		break;
	case TimelineChannel::Color3Val:
		{
			Color3 c = nif->get<Color3>( keyIdx, "Value" );
			if ( comp == 0 ) c.setRed( v ); else if ( comp == 1 ) c.setGreen( v ); else c.setBlue( v );
			tlPushTypedChange<Color3>( nif, iVal, c );
		}
		break;
	case TimelineChannel::Color4Val:
		{
			Color4 c = nif->get<Color4>( keyIdx, "Value" );
			if ( comp == 0 ) c.setRed( v ); else if ( comp == 1 ) c.setGreen( v );
			else if ( comp == 2 ) c.setBlue( v ); else c.setAlpha( v );
			tlPushTypedChange<Color4>( nif, iVal, c );
		}
		break;
	case TimelineChannel::BoolVal:
		pushFieldChange( iVal, (int)( v >= 0.5f ? 1 : 0 ) );
		break;
	case TimelineChannel::QuatVal:
		{
			Quat q = nif->get<Quat>( keyIdx, "Value" );
			q[std::clamp( comp, 0, 3 )] = v;
			tlPushTypedChange<Quat>( nif, iVal, q );
		}
		break;
	default:
		break;
	}
}

float TimelineWidget::keyTangentValue( const TimelineChannel & ch, const QModelIndex & keyIdx, int comp, bool backward ) const
{
	if ( !nif || !keyIdx.isValid() )
		return 0;

	const char * field = backward ? "Backward" : "Forward";

	switch ( ch.type ) {
	case TimelineChannel::Float:
		return nif->get<float>( keyIdx, field );
	case TimelineChannel::Vector3Val:
		return nif->get<Vector3>( keyIdx, field )[std::clamp( comp, 0, 2 )];
	case TimelineChannel::Color4Val:
		{
			Color4 c = nif->get<Color4>( keyIdx, field );
			return comp == 0 ? c.red() : comp == 1 ? c.green() : comp == 2 ? c.blue() : c.alpha();
		}
	default:
		return 0;
	}
}

void TimelineWidget::setKeyTangent( const TimelineChannel & ch, const QModelIndex & keyIdx, int comp, bool backward, float v )
{
	if ( !nif || !keyIdx.isValid() )
		return;

	const char * field = backward ? "Backward" : "Forward";
	QModelIndex iField = nif->getIndex( keyIdx, field );
	if ( !iField.isValid() )
		return;

	switch ( ch.type ) {
	case TimelineChannel::Float:
		tlPushTypedChange<float>( nif, iField, v );
		break;
	case TimelineChannel::Vector3Val:
		{
			Vector3 vec = nif->get<Vector3>( keyIdx, field );
			vec[std::clamp( comp, 0, 2 )] = v;
			tlPushTypedChange<Vector3>( nif, iField, vec );
		}
		break;
	case TimelineChannel::Color4Val:
		{
			Color4 c = nif->get<Color4>( keyIdx, field );
			if ( comp == 0 ) c.setRed( v ); else if ( comp == 1 ) c.setGreen( v );
			else if ( comp == 2 ) c.setBlue( v ); else c.setAlpha( v );
			tlPushTypedChange<Color4>( nif, iField, c );
		}
		break;
	default:
		break;
	}
}

void TimelineWidget::setKeyTime( const TimelineChannel & ch, int keyIndex, float t )
{
	if ( keyIndex < 0 || keyIndex >= ch.keys.count() )
		return;

	QModelIndex iTime = nif->getIndex( QModelIndex( ch.keys[keyIndex].idx ), "Time" );
	if ( iTime.isValid() )
		pushFieldChange( iTime, t );
}

float TimelineWidget::clampKeyTime( const TimelineChannel & ch, int keyIndex, float t ) const
{
	// clamp against nearest *unselected* neighbors so group drags stay rigid
	const float eps = 1.0e-4f;

	for ( int i = keyIndex - 1; i >= 0; i-- ) {
		if ( !selKeys.contains( ch.keys[i].idx ) ) {
			t = std::max( t, ch.keys[i].time + eps );
			break;
		}
	}

	for ( int i = keyIndex + 1; i < ch.keys.count(); i++ ) {
		if ( !selKeys.contains( ch.keys[i].idx ) ) {
			t = std::min( t, ch.keys[i].time - eps );
			break;
		}
	}

	return t;
}

/*
 *  Structural operations (snapshot based undo)
 */

bool TimelineWidget::snapshotOp( const QString & description, const std::function<void()> & op )
{
	if ( !nif )
		return false;

	QByteArray before;
	{
		QBuffer buf( &before );
		buf.open( QIODevice::WriteOnly );
		if ( !nif->save( buf ) )
			return false;
	}

	op();

	QByteArray after;
	{
		QBuffer buf( &after );
		buf.open( QIODevice::WriteOnly );
		if ( !nif->save( buf ) ) {
			refreshLater();
			return false;
		}
	}

	nif->undoStack->push( new NifSnapshotCommand( nif, before, after, description ) );
	refreshLater();
	return true;
}

QVector<TimelineKeyData> TimelineWidget::readChannelKeys( const TimelineChannel & ch ) const
{
	QVector<TimelineKeyData> out;
	if ( !nif )
		return out;

	for ( const auto & key : ch.keys ) {
		QModelIndex keyRow( key.idx );
		TimelineKeyData kd;
		kd.time = nif->get<float>( keyRow, "Time" );

		int nc = std::max( ch.numComponents, 1 );
		for ( int c = 0; c < nc; c++ )
			kd.comps.append( keyComponentValue( ch, keyRow, c ) );

		if ( ch.interpolation == 2 ) {
			for ( int c = 0; c < nc; c++ ) {
				kd.fwd.append( keyTangentValue( ch, keyRow, c, false ) );
				kd.bwd.append( keyTangentValue( ch, keyRow, c, true ) );
			}
		}

		if ( ch.interpolation == 3 ) {
			QModelIndex iTBC = nif->getIndex( keyRow, "TBC" );
			kd.tbc[0] = nif->get<float>( iTBC, "t" );
			kd.tbc[1] = nif->get<float>( iTBC, "b" );
			kd.tbc[2] = nif->get<float>( iTBC, "c" );
		}

		if ( ch.type == TimelineChannel::TextVal )
			kd.text = key.text;

		out.append( kd );
	}

	return out;
}

void TimelineWidget::writeChannelKeys( const TimelineChannel & ch, const QVector<TimelineKeyData> & keys, int newInterp )
{
	if ( !nif )
		return;

	QModelIndex iArray( ch.iKeysArray );
	if ( !iArray.isValid() )
		return;

	int interp = ( newInterp > 0 ) ? newInterp : ch.interpolation;

	// Update the count / interpolation fields depending on where this array lives
	if ( ch.iKeyGroup.isValid() ) {
		QModelIndex iGroup( ch.iKeyGroup );
		nif->set<int>( iGroup, "Num Keys", keys.count() );
		if ( keys.count() > 0 )
			nif->set<int>( iGroup, "Interpolation", interp );
	} else {
		QModelIndex iBlockParent = iArray.parent();
		if ( ch.type == TimelineChannel::QuatVal ) {
			nif->set<int>( iBlockParent, "Num Rotation Keys", keys.count() );
			if ( keys.count() > 0 )
				nif->set<int>( iBlockParent, "Rotation Type", interp );
		} else if ( ch.type == TimelineChannel::TextVal ) {
			nif->set<int>( iBlockParent, "Num Text Keys", keys.count() );
		}
	}

	nif->updateArraySize( iArray );

	int nc = std::max( ch.numComponents, 1 );

	for ( int k = 0; k < keys.count(); k++ ) {
		QModelIndex keyRow = nif->getIndex( iArray, k );
		const TimelineKeyData & kd = keys[k];

		nif->set<float>( keyRow, "Time", kd.time );

		switch ( ch.type ) {
		case TimelineChannel::Float:
			nif->set<float>( keyRow, "Value", kd.comps.value( 0 ) );
			break;
		case TimelineChannel::Vector3Val:
			nif->set<Vector3>( keyRow, "Value", Vector3( kd.comps.value( 0 ), kd.comps.value( 1 ), kd.comps.value( 2 ) ) );
			break;
		case TimelineChannel::Color3Val:
			{
				Color3 c;
				c.setRGB( kd.comps.value( 0 ), kd.comps.value( 1 ), kd.comps.value( 2 ) );
				nif->set<Color3>( keyRow, "Value", c );
			}
			break;
		case TimelineChannel::Color4Val:
			{
				Color4 c;
				c.setRGBA( kd.comps.value( 0 ), kd.comps.value( 1 ), kd.comps.value( 2 ), kd.comps.value( 3, 1.0f ) );
				nif->set<Color4>( keyRow, "Value", c );
			}
			break;
		case TimelineChannel::BoolVal:
			nif->set<int>( keyRow, "Value", kd.comps.value( 0 ) >= 0.5f ? 1 : 0 );
			break;
		case TimelineChannel::QuatVal:
			{
				Quat q;
				for ( int c = 0; c < 4; c++ )
					q[c] = kd.comps.value( c );
				nif->set<Quat>( keyRow, "Value", q );
			}
			break;
		case TimelineChannel::TextVal:
			nif->assignString( keyRow, QStringLiteral( "Value" ), kd.text, false );
			break;
		default:
			break;
		}

		if ( interp == 2 && ch.type != TimelineChannel::TextVal && ch.type != TimelineChannel::QuatVal ) {
			switch ( ch.type ) {
			case TimelineChannel::Float:
			case TimelineChannel::BoolVal:
				nif->set<float>( keyRow, "Forward", kd.fwd.value( 0 ) );
				nif->set<float>( keyRow, "Backward", kd.bwd.value( 0 ) );
				break;
			case TimelineChannel::Vector3Val:
				nif->set<Vector3>( keyRow, "Forward", Vector3( kd.fwd.value( 0 ), kd.fwd.value( 1 ), kd.fwd.value( 2 ) ) );
				nif->set<Vector3>( keyRow, "Backward", Vector3( kd.bwd.value( 0 ), kd.bwd.value( 1 ), kd.bwd.value( 2 ) ) );
				break;
			case TimelineChannel::Color3Val:
				{
					Color3 f, b;
					f.setRGB( kd.fwd.value( 0 ), kd.fwd.value( 1 ), kd.fwd.value( 2 ) );
					b.setRGB( kd.bwd.value( 0 ), kd.bwd.value( 1 ), kd.bwd.value( 2 ) );
					nif->set<Color3>( keyRow, "Forward", f );
					nif->set<Color3>( keyRow, "Backward", b );
				}
				break;
			case TimelineChannel::Color4Val:
				{
					Color4 f, b;
					f.setRGBA( kd.fwd.value( 0 ), kd.fwd.value( 1 ), kd.fwd.value( 2 ), kd.fwd.value( 3, 0.0f ) );
					b.setRGBA( kd.bwd.value( 0 ), kd.bwd.value( 1 ), kd.bwd.value( 2 ), kd.bwd.value( 3, 0.0f ) );
					nif->set<Color4>( keyRow, "Forward", f );
					nif->set<Color4>( keyRow, "Backward", b );
				}
				break;
			default:
				break;
			}
		}

		if ( interp == 3 ) {
			QModelIndex iTBC = nif->getIndex( keyRow, "TBC" );
			if ( iTBC.isValid() ) {
				nif->set<float>( iTBC, "t", kd.tbc[0] );
				nif->set<float>( iTBC, "b", kd.tbc[1] );
				nif->set<float>( iTBC, "c", kd.tbc[2] );
			}
		}
	}
	Q_UNUSED( nc );
}

/*
 *  Key operations
 */

void TimelineWidget::deleteSelectedKeys()
{
	if ( !nif || selKeys.isEmpty() )
		return;

	// group selected keys by channel
	QHash<QPair<int, int>, QVector<int>> byChannel;
	for ( const auto & k : selKeys ) {
		int lane, ch, key;
		if ( findKeyRef( QModelIndex( k ), lane, ch, key ) && !lanes[lane].locked )
			byChannel[{ lane, ch }].append( key );
	}

	if ( byChannel.isEmpty() )
		return;

	// snapshot data first (indices go stale after edits)
	QVector<QPair<QPair<int, int>, QVector<TimelineKeyData>>> newData;
	for ( auto it = byChannel.begin(); it != byChannel.end(); ++it ) {
		const TimelineChannel & ch = lanes[it.key().first].channels[it.key().second];
		auto keys = readChannelKeys( ch );
		QVector<int> del = it.value();
		std::sort( del.begin(), del.end(), std::greater<int>() );
		for ( int d : del ) {
			if ( d >= 0 && d < keys.count() )
				keys.removeAt( d );
		}
		newData.append( { it.key(), keys } );
	}

	snapshotOp( tr( "Delete %1 key(s)" ).arg( selKeys.count() ), [this, &newData]() {
		for ( const auto & pair : newData )
			writeChannelKeys( lanes[pair.first.first].channels[pair.first.second], pair.second );
	} );

	selKeys.clear();
	primaryKey = QPersistentModelIndex();
}

void TimelineWidget::clearChannelKeys( int lane )
{
	if ( !nif || lane < 0 || lane >= lanes.count() || lanes[lane].locked )
		return;

	if ( QMessageBox::question( this, tr( "Clear all keys" ),
		tr( "Remove all keys from \"%1\"?" ).arg( lanes[lane].label ) ) != QMessageBox::Yes )
		return;

	snapshotOp( tr( "Clear keys of %1" ).arg( lanes[lane].label ), [this, lane]() {
		for ( const auto & ch : lanes[lane].channels )
			writeChannelKeys( ch, {} );
	} );
}

void TimelineWidget::insertKeyAtTime( int lane, float time )
{
	if ( !nif || lane < 0 || lane >= lanes.count() || lanes[lane].locked )
		return;

	const TimelineLane & l = lanes[lane];

	QVector<QPair<int, QVector<TimelineKeyData>>> newData;

	for ( int c = 0; c < l.channels.count(); c++ ) {
		const TimelineChannel & ch = l.channels[c];
		if ( !ch.plottable() || ch.type == TimelineChannel::QuatVal )
			continue;

		auto keys = readChannelKeys( ch );

		// skip channels that already have a key at this time
		bool dup = false;
		for ( const auto & kd : keys ) {
			if ( std::abs( kd.time - time ) < 1.0e-4f )
				dup = true;
		}
		if ( dup )
			continue;

		TimelineKeyData nk;
		nk.time = time;
		int nc = std::max( ch.numComponents, 1 );

		// sample current curve value so inserting does not change the shape (for linear)
		QModelIndex iKeyGroup( ch.iKeyGroup );
		int last = 0;
		for ( int comp = 0; comp < nc; comp++ ) {
			float v = 0;
			switch ( ch.type ) {
			case TimelineChannel::Float:
				{
					float fv = 0;
					Controller::interpolate( fv, iKeyGroup, time, last );
					v = fv;
				}
				break;
			case TimelineChannel::Vector3Val:
				{
					Vector3 vec;
					Controller::interpolate( vec, iKeyGroup, time, last );
					v = vec[comp];
				}
				break;
			case TimelineChannel::Color3Val:
				{
					Color3 col;
					Controller::interpolate( col, iKeyGroup, time, last );
					v = comp == 0 ? col.red() : comp == 1 ? col.green() : col.blue();
				}
				break;
			case TimelineChannel::Color4Val:
				{
					Color4 col;
					Controller::interpolate( col, iKeyGroup, time, last );
					v = comp == 0 ? col.red() : comp == 1 ? col.green() : comp == 2 ? col.blue() : col.alpha();
				}
				break;
			case TimelineChannel::BoolVal:
				{
					bool bv = false;
					Controller::interpolate( bv, iKeyGroup, time, last );
					v = bv ? 1.0f : 0.0f;
				}
				break;
			default:
				break;
			}
			nk.comps.append( v );
		}

		if ( ch.interpolation == 2 ) {
			for ( int comp = 0; comp < nc; comp++ ) {
				nk.fwd.append( 0 );
				nk.bwd.append( 0 );
			}
		}

		int at = 0;
		while ( at < keys.count() && keys[at].time < time )
			at++;
		keys.insert( at, nk );

		newData.append( { c, keys } );
	}

	if ( newData.isEmpty() )
		return;

	snapshotOp( tr( "Insert key at %1" ).arg( time ), [this, lane, &newData]() {
		for ( const auto & pair : newData )
			writeChannelKeys( lanes[lane].channels[pair.first], pair.second );
	} );
}

void TimelineWidget::duplicateSelectedKeys()
{
	if ( !nif || selKeys.isEmpty() )
		return;

	float offset = framesMode ? 1.0f / std::max( fps, 1 ) : snapTimeStep;

	QHash<QPair<int, int>, QVector<int>> byChannel;
	for ( const auto & k : selKeys ) {
		int lane, ch, key;
		if ( findKeyRef( QModelIndex( k ), lane, ch, key ) && !lanes[lane].locked )
			byChannel[{ lane, ch }].append( key );
	}

	QVector<QPair<QPair<int, int>, QVector<TimelineKeyData>>> newData;
	for ( auto it = byChannel.begin(); it != byChannel.end(); ++it ) {
		const TimelineChannel & ch = lanes[it.key().first].channels[it.key().second];
		auto keys = readChannelKeys( ch );
		QVector<TimelineKeyData> dups;
		for ( int idx : it.value() ) {
			if ( idx >= 0 && idx < keys.count() ) {
				TimelineKeyData kd = keys[idx];
				kd.time += offset;
				dups.append( kd );
			}
		}
		for ( const auto & kd : dups ) {
			int at = 0;
			while ( at < keys.count() && keys[at].time < kd.time )
				at++;
			keys.insert( at, kd );
		}
		newData.append( { it.key(), keys } );
	}

	snapshotOp( tr( "Duplicate %1 key(s)" ).arg( selKeys.count() ), [this, &newData]() {
		for ( const auto & pair : newData )
			writeChannelKeys( lanes[pair.first.first].channels[pair.first.second], pair.second );
	} );
}

void TimelineWidget::copySelectedKeys()
{
	keyClipboard.clear();
	if ( selKeys.isEmpty() )
		return;

	float refTime = 0;
	bool first = true;

	QHash<QPair<int, int>, QVector<int>> byChannel;
	for ( const auto & k : selKeys ) {
		int lane, ch, key;
		if ( findKeyRef( QModelIndex( k ), lane, ch, key ) ) {
			byChannel[{ lane, ch }].append( key );
			float t = lanes[lane].channels[ch].keys[key].time;
			if ( first || t < refTime ) {
				refTime = t;
				first = false;
			}
		}
	}

	for ( auto it = byChannel.begin(); it != byChannel.end(); ++it ) {
		const TimelineChannel & ch = lanes[it.key().first].channels[it.key().second];
		auto all = readChannelKeys( ch );

		TimelineClipChannel cc;
		cc.type = ch.type;
		cc.interpolation = ch.interpolation;
		cc.numComponents = ch.numComponents;
		cc.name = ch.name;

		QVector<int> sorted = it.value();
		std::sort( sorted.begin(), sorted.end() );
		for ( int idx : sorted ) {
			if ( idx >= 0 && idx < all.count() )
				cc.keys.append( all[idx] );
		}

		keyClipboard.append( cc );
	}

	keyClipboardRefTime = refTime;
	infoLabel->setText( tr( "Copied %1 key(s)" ).arg( selKeys.count() ) );
}

void TimelineWidget::pasteKeysAt( float time )
{
	if ( !nif || keyClipboard.isEmpty() || currentLane < 0 || currentLane >= lanes.count() || lanes[currentLane].locked )
		return;

	const TimelineLane & l = lanes[currentLane];

	QVector<QPair<int, QVector<TimelineKeyData>>> newData;
	QVector<bool> used( l.channels.count(), false );

	for ( const auto & cc : keyClipboard ) {
		// find a compatible unused channel
		int target = -1;
		for ( int c = 0; c < l.channels.count(); c++ ) {
			if ( !used[c] && l.channels[c].type == cc.type && l.channels[c].numComponents == cc.numComponents ) {
				target = c;
				break;
			}
		}
		if ( target < 0 )
			continue;
		used[target] = true;

		auto keys = readChannelKeys( l.channels[target] );

		for ( TimelineKeyData kd : cc.keys ) {
			kd.time = kd.time - keyClipboardRefTime + time;
			// remove any existing key at the same time
			for ( int i = keys.count() - 1; i >= 0; i-- ) {
				if ( std::abs( keys[i].time - kd.time ) < 1.0e-4f )
					keys.removeAt( i );
			}
			int at = 0;
			while ( at < keys.count() && keys[at].time < kd.time )
				at++;
			keys.insert( at, kd );
		}

		newData.append( { target, keys } );
	}

	if ( newData.isEmpty() ) {
		QMessageBox::information( this, tr( "Paste keys" ), tr( "No compatible channel on this lane." ) );
		return;
	}

	snapshotOp( tr( "Paste keys" ), [this, &newData]() {
		for ( const auto & pair : newData )
			writeChannelKeys( lanes[currentLane].channels[pair.first], pair.second );
	} );
}

void TimelineWidget::copyChannels( int lane )
{
	if ( lane < 0 || lane >= lanes.count() )
		return;

	channelClipboard.clear();

	for ( const auto & ch : lanes[lane].channels ) {
		if ( !ch.plottable() )
			continue;
		TimelineClipChannel cc;
		cc.type = ch.type;
		cc.interpolation = ch.interpolation;
		cc.numComponents = ch.numComponents;
		cc.name = ch.name;
		cc.keys = readChannelKeys( ch );
		channelClipboard.append( cc );
	}

	infoLabel->setText( tr( "Copied %1 channel(s)" ).arg( channelClipboard.count() ) );
}

void TimelineWidget::pasteChannels( int lane )
{
	if ( !nif || lane < 0 || lane >= lanes.count() || channelClipboard.isEmpty() || lanes[lane].locked )
		return;

	const TimelineLane & l = lanes[lane];

	QVector<QPair<int, const TimelineClipChannel *>> matches;
	QVector<bool> used( l.channels.count(), false );

	for ( const auto & cc : channelClipboard ) {
		for ( int c = 0; c < l.channels.count(); c++ ) {
			if ( !used[c] && l.channels[c].type == cc.type && l.channels[c].numComponents == cc.numComponents ) {
				used[c] = true;
				matches.append( { c, &cc } );
				break;
			}
		}
	}

	if ( matches.isEmpty() ) {
		QMessageBox::information( this, tr( "Paste channels" ),
			tr( "No compatible channels on this lane (types must match)." ) );
		return;
	}

	snapshotOp( tr( "Paste channel keys onto %1" ).arg( l.label ), [this, lane, &matches]() {
		for ( const auto & m : matches )
			writeChannelKeys( lanes[lane].channels[m.first], m.second->keys, m.second->interpolation );
	} );
}

void TimelineWidget::scaleSelectedKeys( float factor )
{
	if ( !nif || selKeys.isEmpty() || factor <= 0 )
		return;

	float pivot = curTime;

	QHash<QPair<int, int>, QVector<int>> byChannel;
	for ( const auto & k : selKeys ) {
		int lane, ch, key;
		if ( findKeyRef( QModelIndex( k ), lane, ch, key ) && !lanes[lane].locked )
			byChannel[{ lane, ch }].append( key );
	}

	QVector<QPair<QPair<int, int>, QVector<TimelineKeyData>>> newData;
	for ( auto it = byChannel.begin(); it != byChannel.end(); ++it ) {
		const TimelineChannel & ch = lanes[it.key().first].channels[it.key().second];
		auto keys = readChannelKeys( ch );
		for ( int idx : it.value() ) {
			if ( idx >= 0 && idx < keys.count() )
				keys[idx].time = pivot + ( keys[idx].time - pivot ) * factor;
		}
		std::sort( keys.begin(), keys.end(),
			[]( const TimelineKeyData & a, const TimelineKeyData & b ) { return a.time < b.time; } );
		newData.append( { it.key(), keys } );
	}

	snapshotOp( tr( "Scale %1 key(s)" ).arg( selKeys.count() ), [this, &newData]() {
		for ( const auto & pair : newData )
			writeChannelKeys( lanes[pair.first.first].channels[pair.first.second], pair.second );
	} );
}

void TimelineWidget::applyEasing( int mode )
{
	if ( !nif || selKeys.isEmpty() )
		return;

	ChangeValueCommand::createTransaction();

	for ( const auto & k : selKeys ) {
		int lane, c, key;
		if ( !findKeyRef( QModelIndex( k ), lane, c, key ) || lanes[lane].locked )
			continue;

		const TimelineChannel & ch = lanes[lane].channels[c];
		if ( ch.interpolation != 2 )
			continue;

		QModelIndex keyRow( k );
		int nc = std::max( ch.numComponents, 1 );

		for ( int comp = 0; comp < nc; comp++ ) {
			float vPrev = ( key > 0 ) ? keyComponentValue( ch, QModelIndex( ch.keys[key - 1].idx ), comp ) : 0;
			float vThis = keyComponentValue( ch, keyRow, comp );
			float vNext = ( key + 1 < ch.keys.count() ) ? keyComponentValue( ch, QModelIndex( ch.keys[key + 1].idx ), comp ) : 0;
			bool hasPrev = key > 0;
			bool hasNext = key + 1 < ch.keys.count();

			float fwd = keyTangentValue( ch, keyRow, comp, false );
			float bwd = keyTangentValue( ch, keyRow, comp, true );

			switch ( mode ) {
			case 0: // flatten
				fwd = 0;
				bwd = 0;
				break;
			case 1: // smooth (Catmull-Rom)
				{
					float t = 0.5f * ( ( hasNext ? vNext : vThis ) - ( hasPrev ? vPrev : vThis ) );
					fwd = t;
					bwd = t;
				}
				break;
			case 2: // linearize
				fwd = hasPrev ? ( vThis - vPrev ) : 0;
				bwd = hasNext ? ( vNext - vThis ) : 0;
				break;
			case 3: // ease in: flat departure
				bwd = 0;
				break;
			case 4: // ease out: flat arrival
				fwd = 0;
				break;
			}

			setKeyTangent( ch, keyRow, comp, false, fwd );
			setKeyTangent( ch, keyRow, comp, true, bwd );
		}
	}

	graphView->invalidateCurves();
	lanesView->invalidateStrips();
}

void TimelineWidget::setChannelInterpolation( int lane, int channel, int newType, bool smooth )
{
	if ( !nif || lane < 0 || lane >= lanes.count() || lanes[lane].locked )
		return;
	if ( channel < 0 || channel >= lanes[lane].channels.count() )
		return;

	const TimelineChannel & ch = lanes[lane].channels[channel];
	if ( ch.interpolation == newType || !ch.iKeyGroup.isValid() )
		return;

	auto keys = readChannelKeys( ch );
	int nc = std::max( ch.numComponents, 1 );

	// translate values between the types where possible
	if ( newType == 2 ) {
		// compute tangents that keep the shape of the previous (linear-ish) curve
		for ( int k = 0; k < keys.count(); k++ ) {
			keys[k].fwd.clear();
			keys[k].bwd.clear();
			for ( int comp = 0; comp < nc; comp++ ) {
				float vPrev = ( k > 0 ) ? keys[k - 1].comps.value( comp ) : keys[k].comps.value( comp );
				float vThis = keys[k].comps.value( comp );
				float vNext = ( k + 1 < keys.count() ) ? keys[k + 1].comps.value( comp ) : keys[k].comps.value( comp );

				if ( smooth ) {
					float t = 0.5f * ( vNext - vPrev );
					keys[k].fwd.append( t );
					keys[k].bwd.append( t );
				} else {
					keys[k].fwd.append( vThis - vPrev );
					keys[k].bwd.append( vNext - vThis );
				}
			}
		}
	} else if ( newType == 3 ) {
		for ( auto & kd : keys ) {
			kd.tbc[0] = 0;
			kd.tbc[1] = 0;
			kd.tbc[2] = 0;
		}
	}

	snapshotOp( tr( "Set %1 interpolation to %2" ).arg( ch.name, tlKeyTypeName( newType ) ), [this, lane, channel, &keys, newType]() {
		writeChannelKeys( lanes[lane].channels[channel], keys, newType );
	} );
}

void TimelineWidget::nudgeSelectedKeys( float dt, int dvSteps )
{
	if ( !nif || selKeys.isEmpty() )
		return;

	ChangeValueCommand::createTransaction();

	for ( const auto & k : selKeys ) {
		int lane, c, key;
		if ( !findKeyRef( QModelIndex( k ), lane, c, key ) || lanes[lane].locked )
			continue;

		const TimelineChannel & ch = lanes[lane].channels[c];

		if ( dt != 0.0f ) {
			float nt = clampKeyTime( ch, key, ch.keys[key].time + dt );
			QModelIndex iTime = nif->getIndex( QModelIndex( k ), "Time" );
			if ( iTime.isValid() )
				pushFieldChange( iTime, nt );
		}

		if ( dvSteps != 0 && ch.numComponents == 1 && ch.type != TimelineChannel::BoolVal ) {
			float v = keyComponentValue( ch, QModelIndex( k ), 0 );
			setKeyComponentValue( ch, QModelIndex( k ), 0, v + dvSteps * snapValueStep );
		}
	}

	lanesView->invalidateStrips();
	graphView->invalidateCurves();
	updateViews();
}

void TimelineWidget::toggleLaneMute( int lane )
{
	if ( !nif || lane < 0 || lane >= lanes.count() || !lanes[lane].iController.isValid() )
		return;

	QModelIndex iFlags = nif->getIndex( QModelIndex( lanes[lane].iController ), "Flags" );
	if ( !iFlags.isValid() )
		return;

	quint16 flags = (quint16)nif->get<int>( iFlags );
	flags ^= 0x0008;
	pushFieldChange( iFlags, (int)flags, true );

	lanes[lane].muted = !( flags & 0x0008 );
	lanesView->update();
}

void TimelineWidget::addTextKeyMarker( float time )
{
	if ( !nif )
		return;

	if ( !markerChannel.iKeysArray.isValid() ) {
		QMessageBox::information( this, tr( "Add text key" ),
			tr( "No NiTextKeyExtraData block in the current view. Add one to a sequence first." ) );
		return;
	}

	bool ok = false;
	QString text = QInputDialog::getText( this, tr( "Add text key" ),
		tr( "Text for the key at %1:" ).arg( time ), QLineEdit::Normal, QString(), &ok );
	if ( !ok || text.isEmpty() )
		return;

	auto keys = readChannelKeys( markerChannel );

	TimelineKeyData kd;
	kd.time = time;
	kd.text = text;

	int at = 0;
	while ( at < keys.count() && keys[at].time < time )
		at++;
	keys.insert( at, kd );

	snapshotOp( tr( "Add text key \"%1\"" ).arg( text ), [this, &keys]() {
		writeChannelKeys( markerChannel, keys );
	} );
}

/*
 *  CSV
 */

static QString tlValueTypeName( TimelineChannel::ValueType t )
{
	switch ( t ) {
	case TimelineChannel::Float: return QStringLiteral( "Float" );
	case TimelineChannel::Vector3Val: return QStringLiteral( "Vector3" );
	case TimelineChannel::Color3Val: return QStringLiteral( "Color3" );
	case TimelineChannel::Color4Val: return QStringLiteral( "Color4" );
	case TimelineChannel::BoolVal: return QStringLiteral( "Bool" );
	case TimelineChannel::QuatVal: return QStringLiteral( "Quat" );
	case TimelineChannel::TextVal: return QStringLiteral( "Text" );
	default: return QStringLiteral( "Unknown" );
	}
}

static TimelineChannel::ValueType tlValueTypeFromName( const QString & s )
{
	if ( s == QLatin1String( "Float" ) ) return TimelineChannel::Float;
	if ( s == QLatin1String( "Vector3" ) ) return TimelineChannel::Vector3Val;
	if ( s == QLatin1String( "Color3" ) ) return TimelineChannel::Color3Val;
	if ( s == QLatin1String( "Color4" ) ) return TimelineChannel::Color4Val;
	if ( s == QLatin1String( "Bool" ) ) return TimelineChannel::BoolVal;
	if ( s == QLatin1String( "Quat" ) ) return TimelineChannel::QuatVal;
	if ( s == QLatin1String( "Text" ) ) return TimelineChannel::TextVal;
	return TimelineChannel::Unknown;
}

void TimelineWidget::csvExport( int lane )
{
	if ( !nif || lane < 0 || lane >= lanes.count() )
		return;

	const TimelineLane & l = lanes[lane];

	QString fname = QFileDialog::getSaveFileName( this, tr( "Export keyframes to CSV" ),
		nif->getFolder() + QStringLiteral( "/keys.csv" ), QStringLiteral( "CSV (*.csv)" ) );
	if ( fname.isEmpty() )
		return;

	QFile file( fname );
	if ( !file.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
		QMessageBox::warning( this, tr( "CSV export" ), tr( "Cannot write %1" ).arg( fname ) );
		return;
	}

	QTextStream out( &file );
	out.setRealNumberNotation( QTextStream::FixedNotation );
	out.setRealNumberPrecision( 6 );

	out << "# NifSkope timeline keyframe export\n";
	out << "# This file is self-describing and meant to be edited by hand, script or AI, then\n";
	out << "# imported back onto an interpolator with the timeline's 'Import channel(s) from CSV'.\n";
	out << "# Rules: keep the '# Channel' header lines; times are seconds and must stay ascending\n";
	out << "# within a channel; number of value columns must match Components; extra columns:\n";
	out << "#   Interpolation 2 (Quadratic) adds forward/backward tangent columns per component\n";
	out << "#   Interpolation 3 (TBC) adds tension,bias,continuity columns\n";
	out << "#   ValueType Text uses: time,text\n";
	out << "# Lane: " << l.label << "\n";
	out << "# Block: " << nif->getBlockNumber( QModelIndex( l.iSelect ) ) << " " << nif->itemName( QModelIndex( l.iSelect ) ) << "\n";

	for ( const auto & ch : l.channels ) {
		auto keys = readChannelKeys( ch );
		int nc = std::max( ch.numComponents, 1 );

		out << "#\n";
		out << "# Channel: " << ch.name << "\n";
		out << "# ValueType: " << tlValueTypeName( ch.type ) << "\n";
		out << "# Components: " << nc << "\n";
		out << "# Interpolation: " << ch.interpolation << " (" << tlKeyTypeName( ch.interpolation ) << ")\n";

		// header row
		if ( ch.type == TimelineChannel::TextVal ) {
			out << "time,text\n";
		} else {
			out << "time";
			for ( int c = 0; c < nc; c++ )
				out << ",v" << c;
			if ( ch.interpolation == 2 ) {
				for ( int c = 0; c < nc; c++ )
					out << ",f" << c;
				for ( int c = 0; c < nc; c++ )
					out << ",b" << c;
			}
			if ( ch.interpolation == 3 )
				out << ",tension,bias,continuity";
			out << "\n";
		}

		for ( const auto & kd : keys ) {
			out << kd.time;
			if ( ch.type == TimelineChannel::TextVal ) {
				QString esc = kd.text;
				esc.replace( QLatin1Char( '"' ), QStringLiteral( "\"\"" ) );
				out << ",\"" << esc << "\"";
			} else {
				for ( int c = 0; c < nc; c++ )
					out << "," << kd.comps.value( c );
				if ( ch.interpolation == 2 ) {
					for ( int c = 0; c < nc; c++ )
						out << "," << kd.fwd.value( c );
					for ( int c = 0; c < nc; c++ )
						out << "," << kd.bwd.value( c );
				}
				if ( ch.interpolation == 3 )
					out << "," << kd.tbc[0] << "," << kd.tbc[1] << "," << kd.tbc[2];
			}
			out << "\n";
		}
	}

	infoLabel->setText( tr( "Exported to %1" ).arg( QFileInfo( fname ).fileName() ) );
}

void TimelineWidget::csvImport( int lane )
{
	if ( !nif || lane < 0 || lane >= lanes.count() || lanes[lane].locked )
		return;

	QString fname = QFileDialog::getOpenFileName( this, tr( "Import keyframes from CSV" ),
		nif->getFolder(), QStringLiteral( "CSV (*.csv)" ) );
	if ( fname.isEmpty() )
		return;

	QFile file( fname );
	if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		QMessageBox::warning( this, tr( "CSV import" ), tr( "Cannot read %1" ).arg( fname ) );
		return;
	}

	// parse sections
	struct Section
	{
		QString name;
		TimelineChannel::ValueType type = TimelineChannel::Unknown;
		int comps = 1;
		int interp = 1;
		QVector<TimelineKeyData> keys;
	};

	QVector<Section> sections;
	Section cur;
	bool inSection = false;

	QTextStream in( &file );
	int lineNo = 0;

	auto flush = [&]() {
		if ( inSection && !cur.keys.isEmpty() )
			sections.append( cur );
		cur = Section();
		inSection = false;
	};

	while ( !in.atEnd() ) {
		QString line = in.readLine().trimmed();
		lineNo++;

		if ( line.isEmpty() )
			continue;

		if ( line.startsWith( QLatin1Char( '#' ) ) ) {
			QString body = line.mid( 1 ).trimmed();
			if ( body.startsWith( QLatin1String( "Channel:" ) ) ) {
				flush();
				inSection = true;
				cur.name = body.mid( 8 ).trimmed();
			} else if ( body.startsWith( QLatin1String( "ValueType:" ) ) ) {
				cur.type = tlValueTypeFromName( body.mid( 10 ).trimmed() );
			} else if ( body.startsWith( QLatin1String( "Components:" ) ) ) {
				cur.comps = body.mid( 11 ).trimmed().toInt();
			} else if ( body.startsWith( QLatin1String( "Interpolation:" ) ) ) {
				cur.interp = body.mid( 14 ).trimmed().section( ' ', 0, 0 ).toInt();
			}
			continue;
		}

		if ( !inSection )
			continue;

		if ( line.startsWith( QLatin1String( "time" ) ) )
			continue; // header row

		// data row
		TimelineKeyData kd;

		if ( cur.type == TimelineChannel::TextVal ) {
			int comma = line.indexOf( QLatin1Char( ',' ) );
			if ( comma < 0 )
				continue;
			kd.time = line.left( comma ).toFloat();
			QString text = line.mid( comma + 1 ).trimmed();
			if ( text.startsWith( QLatin1Char( '"' ) ) && text.endsWith( QLatin1Char( '"' ) ) && text.length() >= 2 )
				text = text.mid( 1, text.length() - 2 ).replace( QStringLiteral( "\"\"" ), QStringLiteral( "\"" ) );
			kd.text = text;
		} else {
			QStringList parts = line.split( QLatin1Char( ',' ) );
			int expected = 1 + cur.comps + ( cur.interp == 2 ? cur.comps * 2 : 0 ) + ( cur.interp == 3 ? 3 : 0 );
			if ( parts.count() < 1 + cur.comps ) {
				QMessageBox::warning( this, tr( "CSV import" ),
					tr( "Line %1: expected at least %2 columns, got %3." ).arg( lineNo ).arg( 1 + cur.comps ).arg( parts.count() ) );
				return;
			}

			int p = 0;
			kd.time = parts[p++].toFloat();
			for ( int c = 0; c < cur.comps; c++ )
				kd.comps.append( parts.value( p++ ).toFloat() );
			if ( cur.interp == 2 && parts.count() >= expected ) {
				for ( int c = 0; c < cur.comps; c++ )
					kd.fwd.append( parts.value( p++ ).toFloat() );
				for ( int c = 0; c < cur.comps; c++ )
					kd.bwd.append( parts.value( p++ ).toFloat() );
			} else if ( cur.interp == 2 ) {
				for ( int c = 0; c < cur.comps; c++ ) {
					kd.fwd.append( 0 );
					kd.bwd.append( 0 );
				}
			}
			if ( cur.interp == 3 && parts.count() >= expected ) {
				kd.tbc[0] = parts.value( p++ ).toFloat();
				kd.tbc[1] = parts.value( p++ ).toFloat();
				kd.tbc[2] = parts.value( p++ ).toFloat();
			}
		}

		cur.keys.append( kd );
	}
	flush();

	if ( sections.isEmpty() ) {
		QMessageBox::warning( this, tr( "CSV import" ), tr( "No channel sections found in the file." ) );
		return;
	}

	// validate times ascending
	for ( auto & sec : sections ) {
		for ( int i = 1; i < sec.keys.count(); i++ ) {
			if ( sec.keys[i].time < sec.keys[i - 1].time ) {
				QMessageBox::warning( this, tr( "CSV import" ),
					tr( "Channel \"%1\": key times are not ascending (key %2)." ).arg( sec.name ).arg( i + 1 ) );
				return;
			}
		}
	}

	// map to lane channels by name first, then by type
	const TimelineLane & l = lanes[lane];
	QVector<QPair<int, const Section *>> matches;
	QVector<bool> used( l.channels.count(), false );

	for ( const auto & sec : sections ) {
		int target = -1;
		for ( int c = 0; c < l.channels.count(); c++ ) {
			if ( !used[c] && l.channels[c].name == sec.name && l.channels[c].type == sec.type ) {
				target = c;
				break;
			}
		}
		if ( target < 0 ) {
			for ( int c = 0; c < l.channels.count(); c++ ) {
				if ( !used[c] && l.channels[c].type == sec.type && l.channels[c].numComponents == sec.comps ) {
					target = c;
					break;
				}
			}
		}
		if ( target < 0 ) {
			QMessageBox::warning( this, tr( "CSV import" ),
				tr( "No matching channel for \"%1\" (%2) on this lane." ).arg( sec.name, tlValueTypeName( sec.type ) ) );
			return;
		}
		used[target] = true;
		matches.append( { target, &sec } );
	}

	snapshotOp( tr( "Import CSV onto %1" ).arg( l.label ), [this, lane, &matches]() {
		for ( const auto & m : matches )
			writeChannelKeys( lanes[lane].channels[m.first], m.second->keys, m.second->interp );
	} );

	infoLabel->setText( tr( "Imported %1 channel(s)" ).arg( matches.count() ) );
}

/*
 *  Lint
 */

QVector<TimelineLintItem> TimelineWidget::lintScan() const
{
	QVector<TimelineLintItem> out;
	if ( !nif )
		return out;

	// palette names
	QSet<QString> paletteNames;
	QVector<QModelIndex> palettes;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex iBlock = nif->getBlockIndex( b );
		if ( nif->blockInherits( iBlock, "NiDefaultAVObjectPalette" ) ) {
			palettes.append( iBlock );
			QModelIndex iObjs = nif->getIndex( iBlock, "Objs" );
			for ( int r = 0; r < nif->rowCount( iObjs ); r++ )
				paletteNames.insert( nif->resolveString( nif->getIndex( iObjs, r ), "Name" ) );
		}
	}

	// sequences
	for ( const auto & s : sequences ) {
		QModelIndex iSeq( s );
		QString seqName = nif->resolveString( iSeq, "Name" );
		float seqStart = nif->get<float>( iSeq, "Start Time" );
		float seqStop = nif->get<float>( iSeq, "Stop Time" );

		QModelIndex iCtrl = nif->getIndex( iSeq, "Controlled Blocks" );
		for ( int r = 0; r < nif->rowCount( iCtrl ); r++ ) {
			QModelIndex iRow = nif->getIndex( iCtrl, r );
			QString nodeName = nif->resolveString( iRow, "Node Name" );

			if ( !nodeName.isEmpty() && !avObjectsByName.contains( nodeName ) ) {
				TimelineLintItem li;
				li.text = tr( "[%1] Controlled block %2: node \"%3\" does not exist in this file" ).arg( seqName ).arg( r ).arg( nodeName );
				li.idx = iRow;
				li.isNameMismatch = true;
				li.iNameField = nif->getIndex( iRow, "Node Name" );
				out.append( li );
			}

			if ( !nodeName.isEmpty() && !paletteNames.isEmpty() && !paletteNames.contains( nodeName ) ) {
				TimelineLintItem li;
				li.text = tr( "[%1] \"%2\" missing from NiDefaultAVObjectPalette" ).arg( seqName, nodeName );
				li.idx = iRow;
				out.append( li );
			}

			qint32 interpLink = nif->getLink( iRow, "Interpolator" );
			if ( interpLink < 0 ) {
				TimelineLintItem li;
				li.text = tr( "[%1] Controlled block %2 has no interpolator" ).arg( seqName ).arg( r );
				li.idx = iRow;
				out.append( li );
			}
		}

		// missing start/end text keys
		QModelIndex iText = nif->getBlockIndex( nif->getLink( iSeq, "Text Keys" ), "NiTextKeyExtraData" );
		bool hasStart = false, hasEnd = false;
		if ( iText.isValid() ) {
			QModelIndex iKeys = nif->getIndex( iText, "Text Keys" );
			for ( int k = 0; k < nif->rowCount( iKeys ); k++ ) {
				QString t = nif->resolveString( nif->getIndex( iKeys, k ), "Value" );
				if ( t == QLatin1String( "start" ) )
					hasStart = true;
				else if ( t == QLatin1String( "end" ) )
					hasEnd = true;
			}
		}
		if ( !hasStart || !hasEnd ) {
			TimelineLintItem li;
			li.text = tr( "[%1] missing \"%2\" text key" ).arg( seqName, !hasStart ? QStringLiteral( "start" ) : QStringLiteral( "end" ) );
			li.idx = iText.isValid() ? iText : iSeq;
			out.append( li );
		}

		if ( seqStop <= seqStart ) {
			TimelineLintItem li;
			li.text = tr( "[%1] Stop Time (%2) <= Start Time (%3)" ).arg( seqName ).arg( seqStop ).arg( seqStart );
			li.idx = iSeq;
			out.append( li );
		}
	}

	// controllers
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex iBlock = nif->getBlockIndex( b );
		if ( !nif->blockInherits( iBlock, "NiTimeController" ) )
			continue;

		QString cname = QString( "%1 [%2]" ).arg( nif->itemName( iBlock ) ).arg( b );

		if ( nif->getLink( iBlock, "Target" ) < 0 ) {
			TimelineLintItem li;
			li.text = tr( "%1: no target" ).arg( cname );
			li.idx = iBlock;
			out.append( li );
		}

		if ( nif->getIndex( iBlock, "Frequency" ).isValid() && nif->get<float>( iBlock, "Frequency" ) == 0.0f ) {
			TimelineLintItem li;
			li.text = tr( "%1: frequency is 0 (animation will not advance)" ).arg( cname );
			li.idx = nif->getIndex( iBlock, "Frequency" );
			out.append( li );
		}
	}

	// per lane checks: unsorted keys, keys outside controller range
	for ( const auto & lane : lanes ) {
		for ( const auto & ch : lane.channels ) {
			for ( int k = 1; k < ch.keys.count(); k++ ) {
				if ( ch.keys[k].time < ch.keys[k - 1].time ) {
					TimelineLintItem li;
					li.text = tr( "%1 / %2: key times not ascending at key %3" ).arg( lane.label, ch.name ).arg( k + 1 );
					li.idx = QModelIndex( ch.keys[k].idx );
					out.append( li );
					break;
				}
			}
		}

		if ( lane.hasCtrlRange && !lane.rangeOnly ) {
			for ( const auto & key : lane.keys ) {
				if ( key.time < lane.start - 1.0e-4f || key.time > lane.stop + 1.0e-4f ) {
					TimelineLintItem li;
					li.text = tr( "%1: key at %2 outside controller range [%3, %4]" )
						.arg( lane.label ).arg( key.time ).arg( lane.start ).arg( lane.stop );
					li.idx = QModelIndex( key.idx );
					out.append( li );
					break;
				}
			}
		}
	}

	return out;
}

void TimelineWidget::runLint()
{
	auto findings = lintScan();

	QDialog dlg( this );
	dlg.setWindowTitle( tr( "Animation check" ) );
	dlg.resize( 620, 380 );

	auto lay = new QVBoxLayout( &dlg );
	auto list = new QListWidget( &dlg );
	lay->addWidget( new QLabel( findings.isEmpty()
		? tr( "No problems found." )
		: tr( "%1 finding(s). Double-click to select the block; name mismatches offer a fix." ).arg( findings.count() ), &dlg ) );
	lay->addWidget( list );

	for ( const auto & li : findings ) {
		auto item = new QListWidgetItem( li.text, list );
		Q_UNUSED( item );
	}

	connect( list, &QListWidget::itemDoubleClicked, [this, &findings, list, &dlg]( QListWidgetItem * item ) {
		int row = list->row( item );
		if ( row < 0 || row >= findings.count() )
			return;
		const TimelineLintItem & li = findings[row];

		if ( li.isNameMismatch && li.iNameField.isValid() ) {
			// offer to fix the broken name by picking an existing node
			QStringList names = avObjectsByName.keys();
			names.sort( Qt::CaseInsensitive );
			bool ok = false;
			QString pick = QInputDialog::getItem( &dlg, tr( "Fix node name" ),
				tr( "Replace the broken name with:" ), names, 0, false, &ok );
			if ( ok && !pick.isEmpty() ) {
				nif->assignString( QModelIndex( li.iNameField ), pick, false );
				refreshLater();
			}
			return;
		}

		if ( li.idx.isValid() )
			emit indexSelected( QModelIndex( li.idx ) );
	} );

	auto buttons = new QDialogButtonBox( QDialogButtonBox::Close, &dlg );
	connect( buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );
	lay->addWidget( buttons );

	dlg.exec();
}

/*
 *  Settings
 */

void TimelineWidget::loadSettings()
{
	QSettings settings;
	settings.beginGroup( QStringLiteral( "Timeline" ) );
	snapOn = settings.value( QStringLiteral( "Snap" ), false ).toBool();
	snapTimeStep = settings.value( QStringLiteral( "SnapTime" ), 0.05 ).toFloat();
	snapValueStep = settings.value( QStringLiteral( "SnapValue" ), 0.1 ).toFloat();
	framesMode = settings.value( QStringLiteral( "FramesMode" ), false ).toBool();
	fps = settings.value( QStringLiteral( "FPS" ), 30 ).toInt();
	normalized = settings.value( QStringLiteral( "Normalized" ), false ).toBool();
	followPlayhead = settings.value( QStringLiteral( "FollowPlayhead" ), true ).toBool();
	labelW = settings.value( QStringLiteral( "LabelWidth" ), 240 ).toInt();
	bool inspVisible = settings.value( QStringLiteral( "InspectorVisible" ), true ).toBool();
	settings.endGroup();

	if ( btnInspector )
		btnInspector->setChecked( inspVisible );
	if ( inspector )
		inspector->setVisible( inspVisible );
	if ( btnFollow )
		btnFollow->setChecked( followPlayhead );
}

void TimelineWidget::saveSettings() const
{
	QSettings settings;
	settings.beginGroup( QStringLiteral( "Timeline" ) );
	settings.setValue( QStringLiteral( "Snap" ), snapOn );
	settings.setValue( QStringLiteral( "SnapTime" ), (double)snapTimeStep );
	settings.setValue( QStringLiteral( "SnapValue" ), (double)snapValueStep );
	settings.setValue( QStringLiteral( "FramesMode" ), framesMode );
	settings.setValue( QStringLiteral( "FPS" ), fps );
	settings.setValue( QStringLiteral( "Normalized" ), normalized );
	settings.setValue( QStringLiteral( "FollowPlayhead" ), followPlayhead );
	settings.setValue( QStringLiteral( "LabelWidth" ), labelW );
	settings.setValue( QStringLiteral( "InspectorVisible" ), btnInspector ? btnInspector->isChecked() : true );
	settings.endGroup();
}
