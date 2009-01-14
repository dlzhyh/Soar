package soar2d.player.eaters;

import soar2d.Soar2D;
import soar2d.player.MoveInfo;
import soar2d.player.Player;

/**
 * @author voigtjr
 *
 * The base eater class, used for human eaters.
 * Soar eaters extend this class because they might one day need to share code with
 * human eaters.
 */
public class Eater extends Player {	
	private MoveInfo move;

	public Eater( String playerId ) {
		super(playerId) ;
	}
	
	/* (non-Javadoc)
	 * @see soar2d.player.Player#update(soar2d.World, java.awt.Point)
	 */
	public void update(java.awt.Point location) {
		super.update(location);
	}

	/* (non-Javadoc)
	 * @see soar2d.player.Player#getMove()
	 */
	public MoveInfo getMove() {
		// the facing depends on the move
		this.setFacingInt(move.moveDirection);
		return move;
	}

	public boolean getHumanMove() {
		move = Soar2D.wm.getHumanMove(this);

		if (move == null) {
			return false;
		}
		
		return true;
	}
	
}
