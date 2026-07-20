/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

#include "spellbook.h"
#include "model/kfmmodel.h"
#include "model/nifmodel.h"
#include "model/nifproxymodel.h"
#include "model/undocommands.h"
#include "ui/widgets/valueedit.h"
#include "ui/widgets/nifcheckboxlist.h"
#include "qt5compat.hpp"

#include <QItemDelegate> // Inherited
#include <QComboBox>
#include <QCursor>
#include <QGuiApplication>
#include <QIcon>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QListView>
#include <QMessageBox>
#include <QTimer>
#include <QToolTip>


//! @file nifdelegate.cpp NifDelegate

extern void qt_format_text( const QFont & font, const QRectF & _r,
                            int tf, const QString & str, QRectF * brect,
                            int tabstops, int * tabarray, int tabarraylen,
                            QPainter * painter );

class NifDelegate final : public QItemDelegate
{
	SpellBookPtr book;
	//! Row whose Reference-column value is being pressed/dragged (diff mode)
	QPersistentModelIndex wwRefPress;
	//! Cursor-following ghost shown while dragging a reference value. Item
	//! views don't route mouse-move to editorEvent, so a short timer tracks
	//! the cursor instead (same pattern as the hover-out operator menus).
	QTimer * wwRefGhostTimer = nullptr;
	QString wwRefGhostText;

	void startRefGhost( const QString & text )
	{
		wwRefGhostText = text;
		if ( !wwRefGhostTimer ) {
			wwRefGhostTimer = new QTimer( this );
			wwRefGhostTimer->setInterval( 50 );
			connect( wwRefGhostTimer, &QTimer::timeout, this, [this]() {
				// self-heal: a release outside any row never reaches
				// editorEvent, so stop as soon as the button is up
				if ( !( QGuiApplication::mouseButtons() & Qt::LeftButton ) ) {
					wwRefPress = QPersistentModelIndex();
					stopRefGhost();
					return;
				}
				QToolTip::showText( QCursor::pos() + QPoint( 16, 12 ), wwRefGhostText );
			} );
		}
		QToolTip::showText( QCursor::pos() + QPoint( 16, 12 ), wwRefGhostText );
		wwRefGhostTimer->start();
	}

	void stopRefGhost()
	{
		if ( wwRefGhostTimer )
			wwRefGhostTimer->stop();
		QToolTip::hideText();
	}

public:
	NifDelegate( QObject * p, SpellBookPtr sb = 0 ) : QItemDelegate( p ), book( sb )
	{
	}

	// ---- link rows: hover ↗ (jump) / ▾ (pick) glyphs -----------------------

	//! Resolve an index to (NifModel, source index, link value) when it is the
	//! Value cell of a Ref/Ptr row; returns nullptr otherwise.
	static NifModel * linkAt( const QAbstractItemModel * model, const QModelIndex & index,
		QModelIndex & sourceIndex, int & link )
	{
		if ( !index.isValid() || index.column() != NifModel::ValueCol )
			return nullptr;

		NifModel * nif = nullptr;
		sourceIndex = index;
		if ( model->inherits( "NifModel" ) ) {
			nif = static_cast<NifModel *>( const_cast<QAbstractItemModel *>( model ) );
		} else if ( model->inherits( "NifProxyModel" ) ) {
			auto proxy = static_cast<NifProxyModel *>( const_cast<QAbstractItemModel *>( model ) );
			nif = static_cast<NifModel *>( proxy->model() );
			sourceIndex = proxy->mapTo( index );
		}
		if ( !nif || !nif->isLink( sourceIndex ) )
			return nullptr;

		link = nif->getLink( sourceIndex );
		return nif;
	}

	static QRect pickGlyphRect( const QRect & cell )
	{
		return QRect( cell.right() - 17, cell.top(), 17, cell.height() );
	}

	static QRect jumpGlyphRect( const QRect & cell )
	{
		return QRect( cell.right() - 35, cell.top(), 17, cell.height() );
	}

	//! Write the diff reference's value onto this row (one undoable change).
	void applyReferenceValue( QAbstractItemModel * model, const QModelIndex & index ) const
	{
		if ( !model->inherits( "NifModel" ) )
			return;
		NifModel * nif = static_cast<NifModel *>( model );
		const NifItem * item = nif->getItem( index );
		const auto it = nif->diffRefValues.constFind( item );
		if ( !item || it == nif->diffRefValues.constEnd() )
			return;
		if ( item->valueType() != it.value().type() )
			return;
		QModelIndex vi = nif->itemToIndex( item, NifModel::ValueCol );
		if ( !vi.isValid() )
			return;
		ChangeValueCommand::createTransaction();
		nif->undoStack->push( new ChangeValueCommand( vi, item->value(), it.value(), item->name(), nif ) );
	}

	//! ▾ popup: retarget the link to any type-compatible block (or None),
	//! as one undoable value change.
	void execLinkPickMenu( NifModel * nif, const QModelIndex & sourceIndex,
		QAbstractItemModel * viewModel, const QModelIndex & viewIndex, const QPoint & globalPos ) const
	{
		QVariant v = viewIndex.data( Qt::EditRole );
		if ( !v.canConvert<NifValue>() )
			return;
		NifValue nv = v.value<NifValue>();
		if ( nv.type() != NifValue::tLink && nv.type() != NifValue::tUpLink )
			return;

		const int currentLink = nif->getLink( sourceIndex );
		QString templ = nif->itemTempl( sourceIndex );
		if ( templ.isEmpty() || templ == QLatin1String( "TEMPLATE" ) )
			templ = QStringLiteral( "NiObject" );

		QMenu menu;
		QAction * aNone = menu.addAction( QObject::tr( "None" ) );
		aNone->setData( -1 );
		aNone->setCheckable( true );
		aNone->setChecked( currentLink < 0 );
		menu.addSeparator();

		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex iBlock = nif->getBlockIndex( b );
			if ( !iBlock.isValid() )
				continue;
			if ( !nif->blockInherits( iBlock, templ ) && nif->itemName( iBlock ) != templ )
				continue;
			QString label = QString::number( b ) + QLatin1String( " — " ) + nif->itemName( iBlock );
			QString bname = nif->get<QString>( iBlock, "Name" );
			if ( !bname.isEmpty() ) {
				if ( bname.size() > 40 )
					bname = bname.left( 37 ) + QLatin1String( "..." );
				label += QLatin1String( " \"" ) + bname + QLatin1Char( '"' );
			}
			QAction * a = menu.addAction( label );
			a->setData( b );
			a->setCheckable( true );
			a->setChecked( b == currentLink );
		}

		QAction * chosen = menu.exec( globalPos );
		if ( !chosen )
			return;
		const int target = chosen->data().toInt();
		if ( target == currentLink )
			return;

		nv.setLink( target, nullptr, nullptr );
		QVariant v2;
		v2.setValue( nv );
		const QString valueType = viewModel->sibling( viewIndex.row(), 0, viewIndex ).data().toString();
		ChangeValueCommand::createTransaction();
		nif->undoStack->push( new ChangeValueCommand( sourceIndex, v2, nv.toString(), valueType, nif ) );
		viewModel->setData( viewIndex, v2, Qt::EditRole );
	}

	virtual bool editorEvent( QEvent * event, QAbstractItemModel * model, const QStyleOptionViewItem & option, const QModelIndex & index ) override final
	{
		Q_ASSERT( event );
		Q_ASSERT( model );

		// Reference column (diff mode): press picks the value up — a ghost
		// follows the cursor — and releasing on the same row applies it.
		// Handled before the editable guard (the column itself is read-only).
		if ( ( event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease )
			&& static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton
			&& model->inherits( "NifModel" ) ) {
			if ( event->type() == QEvent::MouseButtonPress ) {
				if ( index.column() == NifModel::WwRefCol ) {
					const QString refText = index.data( Qt::DisplayRole ).toString();
					if ( !refText.isEmpty() ) {
						wwRefPress = QPersistentModelIndex( index );
						startRefGhost( refText );
						return true;
					}
				}
				wwRefPress = QPersistentModelIndex();
			} else if ( wwRefPress.isValid() ) {
				const bool sameRow = wwRefPress.row() == index.row()
					&& wwRefPress.parent() == index.parent();
				wwRefPress = QPersistentModelIndex();
				stopRefGhost();
				if ( sameRow ) {
					applyReferenceValue( model, index );
					return true;
				}
			}
		}

		// Ignore indices which are not editable
		//	since this QItemDelegate bypasses the flags.
		if ( !(model->flags( index ) & Qt::ItemIsEditable) )
			return false;

		switch ( event->type() ) {
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:

			// link-row hover glyphs: ↗ jumps to the linked block, ▾ opens the
			// retarget picker. Handled on release; the press still selects.
			if ( static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton ) {
				QModelIndex sourceIndex;
				int link = -1;
				NifModel * nif = linkAt( model, index, sourceIndex, link );
				if ( nif ) {
					const QPoint pos = static_cast<QMouseEvent *>(event)->pos();
					const bool onPick = pickGlyphRect( option.rect ).contains( pos );
					const bool onJump = jumpGlyphRect( option.rect ).contains( pos );
					if ( onPick || onJump ) {
						if ( event->type() == QEvent::MouseButtonRelease ) {
							if ( onPick ) {
								execLinkPickMenu( nif, sourceIndex, model, index,
									static_cast<QMouseEvent *>(event)->globalPosition().toPoint() );
							} else if ( link >= 0 && nif->isValidBlockNumber( link ) ) {
								// the delegate's parent is the NifSkope window
								QMetaObject::invokeMethod( parent(), "select",
									Q_ARG( QModelIndex, nif->getBlockIndex( link ) ) );
							}
						}
						return true;
					}
				}
			}

			if ( static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton
			     && decoRect( option ).contains( static_cast<QMouseEvent *>(event)->pos() ) )
			{
				// Spell Icons in Value column
				SpellPtr spell = SpellBook::lookup( model->data( index, Qt::UserRole ).toString() );
				if ( spell && !spell->icon().isNull() ) {
					// Spell Icon click
					if ( event->type() == QEvent::MouseButtonRelease ) {
						NifModel * nif = nullptr;
						QModelIndex buddy = index;

						if ( model->inherits( "NifModel" ) ) {
							nif = static_cast<NifModel *>( model );
						} else if ( model->inherits( "NifProxyModel" ) ) {
							NifProxyModel * proxy = static_cast<NifProxyModel *>( model );
							nif = static_cast<NifModel *>( proxy->model() );
							buddy = proxy->mapTo( index );
						}

						// Cast Spell for icon which was clicked
						if ( nif && spell->isApplicable( nif, buddy ) ) {
							if ( book )
								book->cast( nif, buddy, spell );
							else
								spell->cast( nif, buddy );
						}
					}

					return true;
				}
			}

			break;
		case QEvent::MouseButtonDblClick:

			if ( static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton ) {
				if ( model->inherits( "NifModel" ) ) {
					NifModel *	nif = static_cast< NifModel * >( model );
					if ( nif->getBSVersion() >= 130 ) {
						const NifItem *	item = nif->getItem( index );
						for ( const NifItem * i = item; i; i = i->parent() ) {
							if ( i->isAbstract() && i->hasName( "Material" ) ) {
								if ( !nif->blockInherits( i, "BSShaderProperty" ) )
									break;
								const QString &	t = i->strType();
								if ( !( t == "BSLayeredMaterial" || t.startsWith( QLatin1StringView( "BSMaterialData" ) ) ) )
									continue;
								if ( !item->hasName( "Is Modified" ) || !nif->get<bool>( item ) ) {
									if ( !nif->get<bool>( i, "Is Modified" ) ) {
										if ( !item->hasName( "Is Modified" ) )
											nif->set<bool>( i, "Is Modified", true );
										QMessageBox::warning( nullptr, "NifSkope warning", QString( "Changes to material data are not saved automatically, use the spell 'Material/Save Edited Material...'" ) );
									}
									break;
								} else {
									if ( QMessageBox::question( nullptr, "NifSkope warning", QString( "Revert changes to material data?" ) ) == QMessageBox::Yes )
										break;
								}
								return true;
							}
						}
					}
				}

				QVariant v = model->data( index, Qt::EditRole );

				if ( v.canConvert<NifValue>() ) {
					NifValue nv = v.value<NifValue>();

					// Yes/No toggle for bool types
					if ( nv.type() == NifValue::tBool ) {
						int oldv = nv.get<int>( nullptr, nullptr );
						nv.set<int>( oldv ? 0 : 1, nullptr, nullptr );
						model->setData( index, nv.toVariant(), Qt::EditRole );
						return true;
					}
				}
			}

			break;
		default:
			break;
		}

		return false;
	}

	virtual void paint( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const override final
	{
		int namerole = (index.isValid() && index.column() == 0) ? Qt::DisplayRole : NifSkopeDisplayRole;

		QString text = index.data( namerole ).toString();
		QVariant decoration = index.data( Qt::DecorationRole );
		QString deco = decoration.toString();
		QString user = index.data( Qt::UserRole ).toString();
		QIcon icon = decoration.canConvert<QIcon>() ? qvariant_cast<QIcon>( decoration ) : QIcon();

		if ( !user.isEmpty() ) {
			// Find the icon for this Spell if one exists
			SpellPtr spell = SpellBook::lookup( user );
			if ( spell )
				icon = spell->icon();
		}

		QStyleOptionViewItem opt = option;

		QRect tRect = opt.rect;
		QRect dRect;

		if ( !icon.isNull() || !deco.isEmpty() ) {
			dRect = decoRect( opt );
			tRect = textRect( opt );
		}

		opt.state |= QStyle::State_Active;
		QPalette::ColorGroup cg = ( opt.state & QStyle::State_Enabled ) ? QPalette::Normal : QPalette::Disabled;

		// Color the field background if the value type is a color
		//	Otherwise normal behavior
		QVariant color = index.data( Qt::BackgroundRole );
		if ( color.canConvert<QColor>() ) {
			painter->fillRect( option.rect, color.value<QColor>() );
			// We painted our own background; drop the selected state so the base
			// drawDisplay() below doesn't repaint the text rect with the grey
			// highlight brush (which was hiding the multi-selection colour).
			opt.state &= ~QStyle::State_Selected;
		} else if ( option.state & QStyle::State_Selected )
			painter->fillRect( option.rect, option.palette.brush( cg, QPalette::Highlight ) );

		painter->save();
		// Honour a custom text colour (e.g. the object-mode selection highlight).
		// Set it on the palette too, since the base drawDisplay() picks the text
		// colour from opt.palette rather than the painter's pen.
		QVariant fg = index.data( Qt::ForegroundRole );
		if ( fg.canConvert<QColor>() ) {
			QColor fgc = fg.value<QColor>();
			painter->setPen( fgc );
			opt.palette.setColor( QPalette::Text, fgc );
			opt.palette.setColor( QPalette::HighlightedText, fgc );
		} else {
			painter->setPen( opt.palette.color( cg, opt.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text ) );
		}
		painter->setFont( opt.font );

		if ( !icon.isNull() )
			icon.paint( painter, dRect );
		else if ( !deco.isEmpty() )
			painter->drawText( dRect, opt.decorationAlignment, deco );

		if ( !text.isEmpty() ) {
			drawDisplay( painter, opt, tRect, text );
			drawFocus( painter, opt, tRect );
		}

		// grey decoded suffix for flag fields ("14 — Hidden, Shadow")
		if ( index.column() == NifModel::ValueCol && !text.isEmpty() ) {
			QString summary = index.data( WwFlagSummaryRole ).toString();
			if ( !summary.isEmpty() ) {
				const int mainW = opt.fontMetrics.horizontalAdvance( text ) + 8;
				QRect sRect = tRect.adjusted( mainW, 0, -2, 0 );
				if ( sRect.width() > 24 ) {
					painter->setPen( QColor( 128, 128, 128 ) );
					painter->drawText( sRect, Qt::AlignLeft | Qt::AlignVCenter,
						opt.fontMetrics.elidedText(
							QStringLiteral( "— " ) + summary, Qt::ElideRight, sRect.width() ) );
				}
			}
		}

		// link rows: quiet hover glyphs at the right edge of the Value cell
		// (↗ jump to target, ▾ pick a new target). Invisible at rest.
		if ( opt.state & QStyle::State_MouseOver ) {
			QModelIndex sourceIndex;
			int link = -1;
			if ( linkAt( index.model(), index, sourceIndex, link ) ) {
				const QRect pickR = pickGlyphRect( opt.rect );
				const QRect jumpR = jumpGlyphRect( opt.rect );
				// backfill so the glyphs stay legible over long value text
				QColor bg = ( opt.state & QStyle::State_Selected )
					? opt.palette.color( cg, QPalette::Highlight )
					: opt.palette.color( cg, ( opt.features & QStyleOptionViewItem::Alternate )
						? QPalette::AlternateBase : QPalette::Base );
				painter->fillRect( link >= 0 ? jumpR.united( pickR ) : pickR, bg );
				painter->setPen( QColor( 154, 154, 154 ) );
				if ( link >= 0 )
					painter->drawText( jumpR, Qt::AlignCenter, QStringLiteral( "↗" ) );
				painter->drawText( pickR, Qt::AlignCenter, QStringLiteral( "▾" ) );
			}
		}

		painter->restore();
	}

	QSize sizeHint( const QStyleOptionViewItem & option, const QModelIndex & index ) const override final
	{
		QString text = index.data( NifSkopeDisplayRole ).toString();
		auto height = option.fontMetrics.lineSpacing() * (text.count( QLatin1Char( '\n' ) ) + 1);
		// Increase height by 25%
		height *= 1.25;

		return {option.fontMetrics.horizontalAdvance( text ), int( height )};
	}

	QWidget * createEditor( QWidget * parent, const QStyleOptionViewItem &, const QModelIndex & index ) const override final
	{
		if ( !index.isValid() )
			return nullptr;

		QVariant v  = index.data( Qt::EditRole );
		QWidget * w = 0;

		if ( v.canConvert<NifValue>() ) {
			NifValue nv = v.value<NifValue>();

			if ( nv.isCount() && index.column() == NifModel::ValueCol ) {
				NifValue::EnumType type = NifValue::enumType( index.sibling( index.row(), NifModel::TypeCol ).data( NifSkopeDisplayRole ).toString() );

				if ( type == NifValue::eFlags ) {
					w = new NifCheckBoxList( parent );
				} else if ( type == NifValue::eDefault ) {
					QComboBox * c = new QComboBox( parent );
					w = c;
					c->setEditable( true );
				}
			}

			if ( !w && ValueEdit::canEdit( nv.type() ) )
				w = new ValueEdit( parent );
		} else if ( getQVariantMetaType( v ) == QMetaType::QString ) {
			QLineEdit * le = new QLineEdit( parent );
			le->setFrame( false );
			w = le;
		}

		if ( w )
			w->installEventFilter( const_cast<NifDelegate *>( this ) );

		return w;
	}

	void setEditorData( QWidget * editor, const QModelIndex & index ) const override final
	{
		ValueEdit * vedit = qobject_cast<ValueEdit *>( editor );
		QComboBox * cedit = qobject_cast<QComboBox *>( editor );
		QLineEdit * ledit = qobject_cast<QLineEdit *>( editor );
		QVariant v = index.data( Qt::EditRole );

		if ( vedit && v.canConvert<NifValue>() ) {
			vedit->setValue( v.value<NifValue>() );
		} else if ( cedit && v.canConvert<NifValue>() && v.value<NifValue>().isCount() ) {
			cedit->clear();
			QString t = index.sibling( index.row(), NifModel::TypeCol ).data( NifSkopeDisplayRole ).toString();
			const NifValue::EnumOptions & eo = NifValue::enumOptionData( t );
			quint32 value = v.value<NifValue>().toCount( nullptr, nullptr );
			QMapIterator<quint32, QPair<QString, QString> > it( eo.o );

			if ( eo.t == NifValue::eDefault && t.endsWith( QLatin1StringView("HavokMaterial") ) ) {
				// sort Havok material enumerations alphabetically instead of by CRC32 hash value
				QMap<QString, bool> matNames;
				while ( it.hasNext() ) {
					it.next();
					matNames.insert( it.value().first, ( value == it.key() ) );
				}
				for ( auto i = matNames.cbegin(); i != matNames.cend(); i++ )
					cedit->addItem( i.key(), i.value() );

			} else {
				while ( it.hasNext() ) {
					it.next();
					bool ok = false;

					if ( eo.t == NifValue::eFlags ) {
						cedit->addItem( it.value().first, ok = ( value & ( 1 << it.key() ) ) );
					} else {
						cedit->addItem( it.value().first, ok = ( value == it.key() ) );
					}
				}
			}

			cedit->setEditText( NifValue::enumOptionName( t, value ) );

			if ( eo.t == NifValue::eFlags ) {
				// Using setCurrentIndex on bitflag enums causes the QComboBox
				// to break when only one option is selected. It returns something
				// other than -1 and in doing so checked option's UI is inactive.
				// See: https://github.com/niftools/nifskope/issues/51
				cedit->setCurrentIndex( -1 );
			} else {
				cedit->setCurrentIndex( cedit->findText( NifValue::enumOptionName( t, value ) ) );
			}


		} else if ( ledit ) {
			ledit->setText( v.toString() );
		}
	}

	void setModelData( QWidget * editor, QAbstractItemModel * model, const QModelIndex & index ) const override final
	{
		Q_ASSERT( model );
		ValueEdit * vedit = qobject_cast<ValueEdit *>( editor );
		QComboBox * cedit = qobject_cast<QComboBox *>( editor );
		QLineEdit * ledit = qobject_cast<QLineEdit *>( editor );
		QVariant v;

		if ( vedit ) {
			v.setValue( vedit->getValue() );

			// Value is unchanged, do not push to Undo Stack or call setData()
			if ( v == index.data( Qt::EditRole ) )
				return;

			if ( model->inherits( "NifModel" ) ) {
				auto valueType = model->sibling( index.row(), 0, index ).data().toString();

				ChangeValueCommand::createTransaction();
				auto nif = static_cast<NifModel *>(model);
				nif->undoStack->push( new ChangeValueCommand( index, v, vedit->getValue().toString(), valueType, nif ) );
			}

			model->setData( index, v, Qt::EditRole );
		} else if ( cedit ) {
			QString t  = index.sibling( index.row(), NifModel::TypeCol ).data( NifSkopeDisplayRole ).toString();
			QVariant v = index.data( Qt::EditRole );
			bool ok;
			quint32 x = NifValue::enumOptionValue( t, cedit->currentText(), &ok );

			if ( !ok )
				x = cedit->currentText().toUInt();

			if ( v.canConvert<NifValue>() ) {
				NifValue nv = v.value<NifValue>();
				nv.setCount( x, nullptr, nullptr );
				v.setValue( nv );

				// Value is unchanged, do not push to Undo Stack or call setData()
				if ( v == index.data( Qt::EditRole ) )
					return;

				if ( model->inherits( "NifModel" ) ) {
					auto valueType = model->sibling( index.row(), 0, index ).data().toString();

					auto nif = static_cast<NifModel *>(model);
					nif->undoStack->push( new ToggleCheckBoxListCommand( index, v, valueType, nif ) );
				}

				model->setData( index, v, Qt::EditRole );
			}
		} else if ( ledit ) {
			v.setValue( ledit->text() );
			model->setData( index, v, Qt::EditRole );
		}
	}

	QRect decoRect( const QStyleOptionViewItem & opt ) const
	{
		// allways upper left
		return QRect( opt.rect.topLeft(), opt.decorationSize );
	}

	QRect textRect( const QStyleOptionViewItem & opt ) const
	{
		return QRect( QPoint( opt.rect.x() + opt.decorationSize.width(), opt.rect.y() ),
		              QSize( opt.rect.width() - opt.decorationSize.width(), opt.rect.height() )
		);
	}
};

QAbstractItemDelegate * NifModel::createDelegate( QObject * parent, SpellBookPtr book )
{
	return new NifDelegate( parent, book );
}

QAbstractItemDelegate * KfmModel::createDelegate( QObject * p )
{
	return new NifDelegate( p );
}
