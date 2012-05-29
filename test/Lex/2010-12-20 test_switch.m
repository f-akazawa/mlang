switch(which)
  case 'periodic'
    g.max = g.max - g.dx;
    g.bdry = @addGhostPeriodic;
    % no need for bdryData

  case 'extrapolate'
    g.bdry = @addGhostExtrapolate;
    g.bdryData.towardZero = 0;

  case 'dirichlet'
    g.bdry = @addGhostDirichlet;
    g.bdryData.lowerValue = 0;
    g.bdryData.upperValue = 1;

  case 'neumann'
    g.bdry = @addGhostNeumann;
    g.bdryData.lowerSlope = 0 * g.dx;
    g.bdryData.upperSlope = 1 * g.dx;

  otherwise
    error('Unknown type of ghost cells %s', which);

end
